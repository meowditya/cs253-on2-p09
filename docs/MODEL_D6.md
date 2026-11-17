# D6 — Cost model for Structure A and Structure B (written before the final measurements)

**Author: Ram Upadhyay (Analyst)** · *draft prepared with Claude, declared in TOOLS.md. This file must be
committed BEFORE `make bench` is run for the report — the commit time is the evidence.*

## 0. Honest chronology

This is not a blind prior. Before it was written, (a) Python prototypes of both structures were run
(they produced the sift-move, rotation and probe counts quoted below as **pilot** values), and
(b) Claude ran short C++ smoke benchmarks (N ≤ 10⁵) on a cloud VM only to check that the harness
works; it saw that the AVL tree was slower than the heap there. (c) On 3 Nov, while verifying the
complete draft, Claude also ran the complete `make bench` matrix once on a shared cloud VM to check the
pipeline end to end; those numbers were discarded and are not in the repository or in REPORT.md, but
they were seen, so this model is not blind to them. No number from (b) or (c) appears in REPORT.md.
Every prediction below is stated as a range or a direction, and is tested against the final runs on our
own machine. A refuted prediction stays in the report with its explanation.

**Revision note (9 Nov, after an external code review; predictions P1–P7 unchanged).** Three mechanism
statements were corrected: the *latest* victim policy can still let a cancelled entry surface (§4); the
purge cost is a scan of all m entries plus Floyd's build-heap on the N live survivors, with a
workload-dependent move coefficient (§4); and "probes" is defined precisely, with rebuild probes counted
separately (§2). These are corrections of explanations, not of predicted numbers. Ask the instructor
whether a model revised after a pilot/smoke run still counts as "written before measuring", and state
the answer in the report.

## 1. Notation

N = live events (the hold model keeps N fixed); m = physical heap entries (N ≤ m ≤ 2N after every operation, D1);
c = K/M = fraction of removals that are cancellations; L = log₂ N.

## 2. Count model — per hold (pop_min + insert, no cancellations)

| Term | Heap (A) | AVL (B) | Source of the prediction |
|---|---|---|---|
| sift-down levels (pop) | ≈ L − 1.5 moves, ~2 comparisons per level | — | Python pilot: 14.7–15.7 moves at N = 10⁵ (L = 16.6) |
| sift-up moves (insert) | ≈ 0 (≤ 0.1) | — | the new time is later than most of the heap; pilot 0.00–0.09 |
| key comparisons | ≈ 2L + 1 | ≈ depth of insert path ≈ L + O(1) (delete-min compares nothing; it follows `left`) | structure of the algorithms |
| hash-table probe sequences | 2 (root lookup reused for erase; one insertion probe) | 0 | code: `pop_min` reuses its lookup |
| probes per sequence | ≈ 1 (consecutive seqs, Fibonacci hashing) | — | pilot 1.00 |
| rebuild probes | amortised, reported separately | 0 | each hold leaves a tombstone, so the table is rebuilt periodically |

*Probes, defined.* `hash_probes` counts every slot inspected by `find`, `insert` and `erase` (the
operations a hold performs). `hash_rehash_probes` counts the slots inspected while re-inserting records
during a table rebuild (triggered when used + tombstones > capacity / 2); it is an amortised cost and is
reported in its own CSV column. P1 below is about `hash_probes`; the report must give both.
| rebalance events | 0 | ≈ 1.0–1.1 per hold, about a third double | pilot 1.037–1.044 for all five distributions |
| nodes visited | — | delete-min ≈ left-spine length (≤ 1.44 L) + insert path (≤ 1.44 L) | D2 |

**Prediction P1.** In the counting build, heap comparisons per hold ≈ 2L ± 3 and `hash_probes` per hold ≈ 2 ± 0.3
for every distribution; AVL rebalance events per hold between 0.9 and 1.2 for every distribution.

## 3. Time model — per hold

time ≈ (memory accesses that miss cache) × miss cost + (instructions) × CPI.

- **Heap:** one contiguous 24-byte-per-entry array; the top levels stay cached. Misses per pop grow
  with the number of levels that do not fit in cache: roughly L − log₂(cache bytes / 24). Plus about 2
  table-slot misses (table 48–192 B per entry, effectively random positions).
- **AVL:** 40-byte nodes reached by index; neighbouring tree nodes are not neighbours in memory once
  the tree has churned, so most visits below the cached top of the tree are likely misses
  (a *hypothesis*, to be tested only with profiler output).

**Prediction P2 (direction).** Heap faster than AVL at every N ≥ 10³ and every distribution.
**Prediction P3 (size).** The AVL/heap time ratio grows with N, from roughly 1.3–2× at N = 10³ to
roughly 1.5–3× at N = 10⁶.
**Prediction P4 (distribution).** Heap time almost independent of the distribution (Jones found the
same for implicit heaps); AVL times differ across distributions by < 20 %.
**Prediction P5 (small N).** The sorted array (oracle) is fastest for very small N (≤ ~30) and loses to
both from N ≈ 100–300 on; the crossover point is machine-dependent.

## 4. Cancellation sweep — per step (one removal + one insert)

heap step ≈ insert + (1 − c)·[stale-root discards + live pop] + c·[1 lookup + flag] + purge share
AVL step ≈ insert + (1 − c)·delete-min + c·delete-by-key (search ≈ L comparisons + possible cascade)

- Purge share (C = cancelled entries still in the heap): a purge runs only when C > N, so m = N + C < 2C. It scans all m entries (one table
  lookup each, erasing the C cancelled records) and then rebuilds the heap with Floyd's method on the
  N live survivors (O(N) moves, at most about 2N). Both parts are O(C), paid by the C cancels since the
  last purge: O(1) amortised per cancel (D1). The Floyd move coefficient depends on the workload (the
  pilot's 0.744 moves per scanned entry came from one workload; measure it, do not assume it).
- Under the **latest** policy each victim is the largest pending key *when it is cancelled*, so it
  usually leaves through a purge. It can still surface as the root — and be discarded by a pop — if every
  smaller live event is popped before the next purge; this is rare but does happen (small N, high c).
  Under **uniform** many cancelled entries surface and are discarded by pops (each costing a sift-down).
- **Prediction P6.** As c rises, the heap's cost per step falls (a flag is cheaper than a sift-down)
  while the AVL's stays roughly flat or rises (delete-by-key ≈ delete-min). The heap's advantage
  therefore grows with c; no crossover is expected under either policy.
- **Prediction P7.** Heap peak physical/live ≤ 2 at every c (D1), approaching 2 at high c.

## 5. What would refute the model

- P1 off by more than the stated range → recount the algorithm (a missed probe or comparison).
- An AVL faster than the heap at any N ≥ 10⁴ → look at the counts first, then the profiler.
- A crossover in the sweep → check whether it is outside the error bars before calling it one.
