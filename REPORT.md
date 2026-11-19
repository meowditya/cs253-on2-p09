# REPORT — Project 9: Event scheduler, heap against balanced tree

Team O(n²) · CS 253 / MA 253 · Aditya Singh (Architect), Gulam Abbas (Structure A), Aryan Arya
(Structure B), Hiransh Anand (Oracle), Sanchit Goyal (Adversary), Ram Upadhyay (Analyst)

> **Where the numbers come from.** Sections marked **[MEASURE]** are written by
> `scripts/fill_report.py` only from files that `make bench` produced on the one machine named in §6.1
> (`results/summary.csv`, `results/raw/`, `results/profiler/`). Sections marked **[DETERMINISTIC]** are
> reproduced exactly by `make test` on any machine. No number is copied from anywhere else. AI use is
> declared in TOOLS.md.

## 1. Problem and interface (Architect)

A discrete-event network simulator keeps a future event set in which most timers are cancelled before
they fire, and it also needs "the next event after time t". Every structure implements
`include/event_queue.h` (contract and invariants in INTERFACE.md):

| Operation | Meaning |
|---|---|
| `insert(t, type, data) → handle` | key = (t, seq); seq assigned by the base class, so keys are unique |
| `cancel(handle) → bool` | true exactly once while the event is pending |
| `pop_min`, `peek_min` | the pending event with the smallest key |
| `successor(t)` | the pending event with the smallest key among those with time > t (strict) |

Design decisions: integer microsecond time (real ties, no floating-point drift); key = (time, seq) so
ties leave FIFO and the order is total (D3); handle = key (the AVL tree cancels by key, the heap checks
seq *and* time); one shared, exact cancel semantics so the differential test can compare everything;
our own integer mapping on `std::mt19937_64` instead of `std::*_distribution` (portable output).

## 2. Structure A — lazy binary heap (Structure A owner)

The heap is an array of entries (time, seq, type, data) ordered by the key (time, seq); the children of
index i are 2i+1 and 2i+2. Sift-up and sift-down use the hole method: the moving entry is held aside,
entries slide into the hole, and it is written once at the end, so a sift of depth d costs d+1 moves
instead of 3d.

```
 heap array (ordered by key)            SeqTable (open addressing, keyed by seq)
 [0] (5,#3)                              slot = (seq * 0x9E3779B97F4A7C15) >> (64 - log2 capacity)
 [1] (7,#1)   [2] (9,#4)                 record: seq -> (time, cancelled flag)
 [3] (8,#2)x  ...                        x = cancelled: still in the array, flagged in the table
```

**Cancellation is lazy.** A binary heap cannot find an entry by handle, so every entry in the array has
a record in `SeqTable`: open addressing with linear probing, Fibonacci hashing of the sequence number,
tombstones on erase, and a rebuild when occupied + tombstones exceed half the capacity. `cancel(h)`
looks the handle up, checks both seq and time, and sets a flag — O(1) expected, no sifting. The entry
stays in the array until it reaches the root, where `pop_min`/`peek_min` discard it, or until a purge.

**Purge.** With L live and C cancelled entries, the heap purges whenever C > L, after every successful
cancel *and* after every pop (a pop lowers L, so it can also break C ≤ L). A purge keeps the live
entries, erases the cancelled records and rebuilds the heap with Floyd's method. Hence the array never
holds more than 2L entries after an operation, and the cost is O(log n) amortised (D1).

**Successor(t)** is a depth-first search that prunes every subtree whose root time is already no better
than the best candidate; it is O(m) in the worst case, which is the heap's honest weakness for this
operation and the AVL tree's strength.

**Decision we defend.** The table records every heap entry, not only cancelled ones, so `cancel` has
exactly the same meaning on both structures (false for an unknown, popped or already cancelled handle)
and the differential test can compare every return value. To keep that cheap, `pop_min` reuses the
slot it found when checking the root to erase the record, so a hold without cancellations costs two
table operations (one lookup-and-erase, one insert), not three.

## 3. Structure B — AVL tree (Structure B owner)

Nodes live in one contiguous pool (a growable array) and refer to each other by 32-bit indices, not
pointers; freed nodes go on a free list and are reused. Index 0 is NIL, a sentinel whose height is −1,
so `height(child)` needs no null test and a leaf has height 0.

`insert` and `cancel` (delete by key) are recursive: they walk down by comparing (time, seq), and on
the way back up every node recomputes its height and is rebalanced if its children's heights differ by
2. `pop_min` deletes along the left spine. `successor(t)` is one root-to-leaf walk that remembers the
last node with time > t: O(log n).

```
   left-heavy at z, left child y not right-heavy: one right rotation

          z                    y
         / \                  / \
        y   D      ==>       x   z
       / \                      / \
      x   C                    C   D
```

If y leans the other way (the "zig-zag" case) y is first rotated left, then z right: a double rotation.
The rule uses strict comparison of the grandchildren's heights, so the equal-height case that arises
only in deletion takes the single rotation. An insertion needs at most one rebalance (D5); a deletion
can need one at every level (the Fibonacci-tree attack in §7 makes it do so). The height is below
1.45·log₂(n+2) (D2).

**Decision we defend.** Because the pool can grow and move during an insertion, the code never keeps a
reference or pointer to a node across a call that may allocate: it holds indices and re-reads the node
afterwards. This is the bug class that indices-in-a-pool invites, and the sanitizer runs cover it.

## 4. Correctness evidence (Oracle owner) **[DETERMINISTIC]**

| Evidence | Result (`make test`) |
|---|---|
| Hand-computed edge-case suites (5 files), checker after every operation | identical on heap, AVL, oracle |
| Generated attack suites (11 files) vs oracle output | identical on heap and AVL |
| Differential test, seeds 1, 2, 3, 10⁶ operations each, live size ≤ 10⁴ | no disagreement; checkers passed |
| Mutation tests: LIFO ties, cancel ignoring time, non-strict successor, no post-pop purge, stale AVL height, duplicate heap entry + orphan record, bad AVL edges (out of range, cycle, shared child, leaked slot), table record moved past an EMPTY slot (unreachable by probing, so `cancel` would fail) | all 8 caught |
| Shrinker on a planted bug | literal `.ops` reproducer; the test re-checks that removing any single operation makes the bug disappear (1-minimal) |
| Command-line hardening (`tests/cli_test.sh`) | out-of-range or malformed arguments exit 2 at once (no hang, no `inf`/`nan` rows); a CSV or stdout write failure (e.g. a full disk) exits 4 instead of reporting success; `plot.py` rejects empty CSVs |
| CSV validation (`tests/plot_test.py`, synthetic fixture) | 15 defects each rejected before any output is written: mismatched warm-up / holds / steps, checksum disagreement, duplicate row, missing queue mate, incomplete matrix, row outside the matrix, inconsistent cancels, out-of-range values, `nan`, wrong build, empty file; exact ties classified as ties |
| Profiler logic (`tests/profile_test.sh`, fake `perf`) | USABLE / UNAVAILABLE / FAILED reported correctly (zero or implausible counts, a failed run, a failed self-check all give FAILED and exit 1); the benchmark exits 3 if perf does not acknowledge enable/disable |
| Full scale: 10⁶ resident events in each structure, drained in order; heap vs AVL on 10⁶ mixed operations at 10⁶ live | passed |
| Sanitizers (ASan + UBSan, leak detection on) over the whole suite | clean |
| Toolchains: g++ 13 / libstdc++, clang 18 / libstdc++, clang 18 / libc++ | all tests pass; identical simulator output |

Unit tests only check the cases someone thought of. The differential test instead runs the same
generated history on the heap, the AVL tree and the oracle in lockstep and compares every return value
and every returned event, so any disagreement on any operation among a million is caught, and the
shrinker cuts the history down to a few lines. The oracle is a sorted array, simple enough to be
checked by reading. Agreement of outputs is still not enough — a structure can answer correctly while
its internals are already corrupt — so the checkers recompute each structure's invariants from the
raw arrays without trusting any stored field: heap order, table/array agreement and probe-path
reachability for A; ordering, recomputed heights, balance and reachability for B (D4 proves the AVL
checker accepts exactly the valid trees). The mutation tests show that each of these checks can fail.

## 5. Application (Architect) **[DETERMINISTIC]**

Stop-and-wait flows with a retransmission timer and an ACK-inactivity watchdog; exactly 10⁶ dispatched
events; integer arithmetic only. Default run (`sim`, seed 1), identical on heap and AVL:

```
inserted=2962238 processed=1000000 cancelled=1907484 pending=54754
rto_cancelled=953742 idle_cancelled=953742
transmissions=1019981 retransmissions=46239 delivered=953742 idle_fired=0 monitors=19
ties=222069 peak_live=59028 end_time_us=1929077
checksum=0xcbd12e722cac3922
monitor_successors=0xebce99f1c00b6707
```

64.4 % of scheduled events are cancelled (model estimate 2q/(1+2q) = 65.5 % at q = 0.95, minus
start-up/end effects); 222 069 consecutive equal timestamps exercise the tie rule; conservation
`inserted = processed + cancelled + pending` is checked after every event.

## 6. Measurements (Analyst)

<!-- MEASURED-A:BEGIN (generated by scripts/fill_report.py — do not edit by hand) -->
### 6.1 Machine **[MEASURE]**

*Not measured yet: run `make bench` on the team machine; it fills this section.*
<!-- MEASURED-A:END -->

### 6.2 Method
Hold model as in Jones (1986): Jones's random-walk initialisation (P(enqueue) = 0.55), warm-up
max(10⁶, 5N) holds, then max(10⁶, N) timed holds with pre-generated increments; one hold = pop_min +
insert. Distributions: Jones Table I as printed, divided by their ideal means (the printed bimodal has
mean 1.42857, not 1 as Jones's footnote says), scaled to mean 1000 µs. Cancellation sweep: one step =
one successful removal + one insertion, exactly K = round(c·M) cancellations, victim uniform or
largest-key, stream generated before timing and replayed identically on A and B. Five seeds per point,
A and B on the same seed, order alternated (AB/BA).

The metric is **throughput** (the handout's): holds per second and steps per second, from the
`holds_per_sec` / `steps_per_sec` columns (CSV version 3), shown as mean ± sample s.d. over runs. A and B
are compared by **paired** throughput ratios heap/AVL on the same workload — same distribution, N,
warm-up, timed length, seed, repeat and mean (`results/paired_ratios.csv` lists every one). Throughput
is recomputed from the exact integers (operations × 10⁹ / elapsed_ns). `results/plot.py` writes nothing
unless every CSV is valid: right version, columns and build; values in range and mutually consistent;
no duplicate run; exactly the prescribed matrix (five seeds, prescribed warm-up and lengths, nothing
missing or extra); the same popped-key checksum for every queue on the same workload.

Operation counts come only from the counting build (`BUILD=count`, never timed); in timing builds all
`counters()` fields stay 0. A few structure statistics stay on in timing builds (heap purges, purged
and stale entries, peak size, table rebuilds; AVL rebalances): each is one integer increment on an
event that already does far more work (a purge, a rotation, a rebuild, a discarded root, an insert's
size check), so their cost is expected to be negligible; it was not measured separately.


<!-- MEASURED-B:BEGIN (generated by scripts/fill_report.py — do not edit by hand) -->
### 6.3–6.8 Predicted vs measured, hold model, small queues, cancellation sweep, quantisation, cache **[MEASURE]**

*Not measured yet: run `make bench` on the team machine; it fills this section.*
<!-- MEASURED-B:END -->

## 7. Adversarial inputs (Adversary) **[DETERMINISTIC]**

| Attack | Target | What it did (`make test`) |
|---|---|---|
| ascending 1..N | AVL rotations | 990 rebalances for N = 1000; 999 980 for N = 10⁶ (rotation-free exactly at powers of two) |
| zigzag 1, N, 2, N−1, … | AVL double rotations | 617 double + 371 single rebalances for N = 1000 |
| Fibonacci tree + delete-min | deletion cascades | built with 0 rotations; then ONE delete-min performs ⌊h/2⌋ rotations (h = 10, 11, 20, 21, 27 → 5, 5, 10, 10, 13; h = 27 has 832 039 nodes) |
| bloat (L = 100, 3000 steps) | lazy-heap growth | 3 100 physical entries without purging vs 200 with purging |
| drain (N = 1000) | post-pop purge check | 500 entries for 1 live event without it vs 1 with it |
| storm, regrow, latesucc, random | ties, capacity reuse, O(m) heap successor | identical results on all three queues |

**Ascending and zigzag (AVL).** Inserting increasing keys always extends the rightmost path, so the
tree becomes right-heavy again and again; by D5 every insertion that does not raise the height of the
whole tree must rotate, so all but about 1.45·log₂ n insertions rotate. Zigzag (1, N, 2, N−1, …) places
each new key on the inside of a subtree, which is the double-rotation case.

**Fibonacci tree + delete-min (AVL).** A Fibonacci tree is the sparsest AVL tree: every internal node
already leans one way (D2's extremal case). Built with every node leaning right, removing the minimum
shortens the short side at the bottom, the rotation there shortens that subtree, and the imbalance
moves up: ⌊h/2⌋ rotations from one deletion, the O(log n) worst case that insertion never has.

**Bloat and drain (lazy heap).** Bloat cancels events with late timestamps and replaces them, so
cancelled entries never reach the root and, without purging, the array grows without bound while the
live size stays L; with the purge rule it stays within 2L (D1). Drain cancels half the events and then
pops the rest: only pops lower L, so a rule that purged after cancels alone would leave C > L — which
is why the heap also checks after every pop.

**Storm, regrow, latesucc, random.** Storm inserts many events at one timestamp (FIFO order among ties
must hold, D3). Regrow fills, empties and refills (capacity and free-list reuse). Latesucc asks for
successors near the largest time, where the heap's pruned search degenerates to O(m). Random mixes
everything from a seed. All give identical results on the three queues.

## 8. Derivations

D1–D6 are separate PDFs in `docs/derivations/` (`D1.pdf` … `D6.pdf`), each naming its author on page 1;
the `.md` files beside them are their sources (`make pdfs` rebuilds them).

| | Author | Statement |
|---|---|---|
| D1 | Gulam Abbas | lazy heap: at most 2L entries; O(log n) amortised with purge after cancel and pop |
| D2 | Aryan Arya | an AVL tree of height h has at least F(h+3) − 1 nodes, so h < 1.45·log₂(n+2) |
| D3 | Aditya Singh | (time, seq) ordering makes the run deterministic; dispatched timestamps never decrease |
| D4 | Hiransh Anand | the AVL checker accepts exactly the valid AVL trees |
| D5 | Sanchit Goyal | at most one rebalance per insertion; ascending input rotates on all but O(log n) insertions |
| D6 | Ram Upadhyay | the cost model and its predictions P1–P7 |

## 9. Limitations and honest notes

- The default simulator keeps about 6·10⁴ events live; the 10⁶-resident requirement is met by the full-scale
  test and the benchmarks, not by the simulator's default run.
- The heap's `successor` is O(m) by design; the simulator calls it only once per simulated 100 ms.
- Expected O(1) hash costs assume the keys spread well; Fibonacci hashing of sequential seqs does, but
  it is not an adversarial guarantee.
- The cost model (D6) was written after exploratory prototype runs and after Claude's one discarded
  run of the full benchmark matrix on a shared VM while verifying the draft (see TOOLS.md and
  MODEL_D6.md §0); it was revised on 9 Nov for three mechanism statements, with predictions unchanged.
