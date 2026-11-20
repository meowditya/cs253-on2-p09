# Project 09: Event scheduler — heap against balanced tree

Team **O(n²)** — CS 253 / MA 253 Data Structures and Algorithms Laboratory, IIT Indore, Autumn 2026

## Team and roles

| Name | Roll no. | GitHub | Role | Module folder | Derivation |
|------|----------|--------|------|---------------|------------|
| Aditya Singh | 250003007 | @meowditya | Architect | `include/`, `app/`, `scripts/` (build, test, demo), `Makefile`, `INTERFACE.md` | D3 |
| Gulam Abbas | 250041016 | @GulamAbbas07 | Structure A owner | `src/structA/` | D1 |
| Aryan Arya | 250001009 | @Coder1713 | Structure B owner | `src/structB/` | D2 |
| Hiransh Anand | 250041018 | @hiransh116 | Oracle owner | `oracle/` (+ `tests/test_oracle.cpp`, `test_checkers_mutations.cpp`, `test_fullscale.cpp`) | D4 |
| Sanchit Goyal | 250041035 | @sanchit1707 | Adversary | `adversary/` (+ `scripts/run_suites.sh`) | D5 |
| Ram Upadhyay | 250001058 | @Ram-ui31 | Analyst | `bench/`, `results/` (+ `scripts/run_bench.sh`, `scripts/profile.sh`, `docs/MODEL_D6.md`) | D6 |

Unit tests live with their module's owner (`tests/test_structA.cpp` → Structure A owner, and so on).

## Summary

A network simulator keeps a *future event set* of timestamped events — ACK arrivals, retransmission
timeouts, watchdog timers — most of which are cancelled before they fire. We implement it twice behind
one interface: **Structure A**, a binary min-heap keyed by (time, seq) with lazy cancellation recorded
in a hash table written from scratch and purged whenever cancelled entries outnumber live ones; and
**Structure B**, an AVL tree keyed by (time, seq) with true deletion and `successor(t)`. Both agree
with a sorted-array oracle on 10⁶-operation differential tests and drive a retransmission-timer
simulator to byte-identical output. Measured with Jones's hold model and a cancellation sweep, the heap
was faster on every seed at every point: 1.48–2.13× AVL's throughput in the hold model, and up to 7.28×
when 95 % of removals are cancellations (REPORT.md §11).

## Requirements

- **OS:** Linux, WSL or macOS. Tested on macOS (Darwin 27.2.0, Apple M4; the benchmark machine) and on
  Ubuntu 24.04.5 LTS (x86-64).
- **Compiler:** any C++17 compiler; the Makefile uses `g++` unless `CXX` is set (`make CXX=clang++`).
  Flags `-std=c++17 -O2 -DNDEBUG -Wall -Wextra -pedantic -Werror`. Tested with Apple clang 21.0.0
  (clang-2100.4.1.1; benchmarks), g++ 13.3.0 (Ubuntu 24.04), g++ 15.2.0, and clang 18 with libstdc++ and
  with libc++ (`docs/verification/`).
- **Build tool:** GNU make.
- **Python 3** (tested with 3.13) for the test scripts, with **matplotlib** (tested with
  3.11.2) for the figures and the CSV-validation test. Standard library otherwise.
- **Optional:** `pandoc` (tested 3.1.3) + XeLaTeX with the DejaVu fonts for `make pdfs`; Linux `perf` or
  `valgrind` ≥ 3.22 for cache counts (`scripts/profile.sh`).
- **Libraries:** none. No library container stands in for a structure under study: the heap, its hash
  table, the AVL tree and the oracle's binary search are ours; `std::vector` is used only as raw storage,
  and `std::set` / `std::unordered_map` appear only in test-data and benchmark-stream generation.

## Build

```bash
make
```

builds every program and test into `build/release/bin/` (under a minute).

## Quick start

The application is the retransmission-timer simulator. A small run (10 flows, 2 000 dispatched events):

```bash
build/release/bin/sim --queue heap --events 2000 --flows 10
```

Expected output (stdout; the queue name and timing go to stderr), identical with `--queue avl` and
`--queue oracle`:

```
format=project9-v2
params flows=10 events=2000 loss_ppm=50000 seed=1 rtt_min_us=10000 rtt_max_us=50000 rto_us=200000 idle_us=5000000 monitor_us=100000
inserted=5710 processed=2000 cancelled=3682 pending=28
rto_cancelled=1841 idle_cancelled=1841
transmissions=1938 retransmissions=87 delivered=1841 idle_fired=0 monitors=72
ties=2 peak_live=31 end_time_us=7277517
checksum=0x1b432a44fb30ac08
monitor_successors=0x108560e00362a343
```

The full-scale run (`build/release/bin/sim`, default parameters: 20 000 flows, 10⁶ dispatched events)
must end with these golden values on every queue and every correct build:

```
checksum=0xcbd12e722cac3922
monitor_successors=0xebce99f1c00b6707
```

`sim --queue heap|avl|oracle [--events E] [--flows F] [--loss-ppm P] [--seed S] [--rtt-min US]
[--rtt-max US] [--rto US] [--idle US] [--monitor US] [--trace]` — the full option list.

**Live demonstration** (`make demo`, about 15 s, from a fresh clone of the `final` tag): the 10⁶-event
run on the heap; the same run on AVL with `cmp` showing byte-identical output; seeds 1–3 traced event by
event on heap, AVL and oracle; 10⁶ events resident at once in each structure; the heap-bloat attack with
and without purging.

## Tests

```bash
make test        # ~1 min
```

runs, in order: the unit tests of every module, 8 mutation tests of the checkers, command-line
hardening, profiler-logic and CSV-validation checks, the 16 adversarial and edge-case suites on all three
queues, the **differential test** — `difftest --seed S --ops 1000000` for **seeds 1, 2 and 3** (live size
≤ 10⁴; heap, AVL and oracle in lockstep, the invariant checkers every 1 000 operations) — a full-scale
test with 10⁶ resident events, and the simulator identical-output checks. Each differential seed prints

```
difftest seed=1 ops=1000000 max_live=10000: PASS (oracle = heap = avl on every result; checkers passed)
```

and a passing run ends with

```
ALL TESTS PASSED
```

`make sanitize` reruns the tests under AddressSanitizer + UndefinedBehaviorSanitizer (~3 min). On a
failure, `difftest` shrinks the history and saves a minimal reproducer as `failures/seed<S>.ops`.

## Evaluation interface (hidden tests)

```bash
build/release/bin/replay --queue heap   < ops.txt > out.txt     # Structure A
build/release/bin/replay --queue avl    < ops.txt > out.txt     # Structure B
build/release/bin/replay --queue oracle < ops.txt > out.txt     # the reference
```

`replay` reads an operation stream from stdin (or from a file named as its last argument) and prints
one line per operation, then `END size=… issued=…`; the output is byte-identical on all three queues.
Format (INTERFACE.md §7): one operation per line, `#` starts a comment.

| Input | Meaning | Output line |
|---|---|---|
| `I <time> <type> <data>` | insert; the k-th `I` (from 0) creates handle H[k] | `I <seq>` |
| `C <k>` | cancel(H[k]) | `C 1` or `C 0` |
| `F <time> <seq>` | cancel the handle (time, seq) | `F 1` or `F 0` |
| `P` | pop_min | `P <time> <seq> <type> <data>` or `P -` |
| `K` | peek_min | `K <time> <seq> <type> <data>` or `K -` |
| `S <t>` | successor(t): smallest pending key with time > t | `S <time> <seq> <type> <data>` or `S -` |

Example:

```bash
printf 'I 5 0 1\nI 3 0 2\nI 5 0 3\nC 0\nK\nS 3\nP\nP\nP\n' | build/release/bin/replay --queue heap
```

```
I 0
I 1
I 2
C 1
K 3 1 0 2
S 5 2 0 3
P 3 1 0 2
P 5 2 0 3
P -
END size=0 issued=3
```

Options: `--check N` runs the invariant checker every N operations; `--stats` prints structure
statistics to stderr. Exit codes everywhere: 0 ok · 2 bad arguments or input (with file and line) · 3 a
check failed · 4 resource or overflow error. When the instructor publishes the required format for
Project 09, this command will follow it; until then this is the format.

## Benchmarks and figures

```bash
scripts/run_all.sh              # build, test, demo, every benchmark, every figure: ~13 min on an Apple M4
scripts/run_all.sh --figures    # only redraw results/figures/*.png from the committed results/raw/*.csv: seconds
```

`run_all.sh` runs `make build`, `make test`, `make demo` and `make bench` one after another (= `make
all`). `make bench` writes `results/raw/*.csv` (one file per experiment, seed in every row), then
`results/plot.py` validates every CSV and writes `results/figures/*.png`, `results/summary.csv` and
`results/paired_ratios.csv`, and `scripts/fill_report.py` copies the numbers into REPORT.md §10.1 and
§11; a cache profile goes to `results/profiler/`. `make bench-quick` (or `scripts/run_all.sh --quick`) is
a ~1-minute smoke version that replaces the committed results with QUICK data (`git checkout results
REPORT.md` restores them). `make pdfs` rebuilds `REPORT.pdf` and the derivation PDFs.

| Program (`build/release/bin/`) | Use |
|---|---|
| `bench_hold` | hold model: `--queue heap\|avl\|oracle\|control --dist exponential\|uniform\|bimodal\|triangular\|biased --n N` |
| `bench_sweep` | cancellation sweep: `--queue heap\|avl --cancel-bp 0..10000 --policy uniform\|latest --n N` |
| `gen` | adversarial streams: `ascending N`, `descending N`, `zigzag N`, `fib-delete H D`, `bloat L STEPS`, `drain N`, `storm N`, `regrow N`, `latesucc N Q`, `random SEED OPS LIVE` |
| `difftest` | `--seed S --ops N [--max-live L] [--check-every K]` |

`make BUILD=count …` builds the operation-counting flavour (`-DEQ_COUNTING=1`) — never timed.

## Repository map

```
include/        the shared interface: event_queue.h · rng.h · canonical.h (hashing) · opstream.h (.ops format)
src/structA/    Structure A: seq_table.{h,cpp} (hash table) · lazy_heap.{h,cpp}
src/structB/    Structure B: avl_queue.{h,cpp}
app/            the application: simulator.{h,cpp} · sim_main.cpp · queue_factory.{h,cpp}
oracle/         sorted-array oracle · invariant checkers · replay · differential test with shrinker
adversary/      gen.cpp · distributions.h (Jones's Table I) · suites/*.ops + *.expected
bench/          hold.cpp · cancel_sweep.cpp · bench_common.h
tests/          unit, mutation, simulator, full-scale, command-line and CSV-validation tests
scripts/        run_all.sh · run_tests.sh · run_suites.sh · check_identical.sh · demo.sh · run_bench.sh · profile.sh · fill_report.py · make_pdfs.sh
results/        raw/ (CSV, one per experiment) · figures/ · profiler/ · plot.py · machine.txt · summary.csv · paired_ratios.csv
docs/           derivations/D1_<roll>.pdf … D6_<roll>.pdf (+ .md sources) · slides/slides.pdf · MODEL_D6.md · verification/
INTERFACE.md    the common interface and the structural invariants
REPORT.md       the report (REPORT.pdf: the same, exported)
TOOLS.md        every AI and other tool used, with representative prompts
```

## Results at a glance

All timings: one run of `make bench` on an Apple M4 (macOS, Apple clang 21.0.0, `-O2`), five seeds per
point. Full tables, figures and the explanation of every refuted prediction: REPORT.md §11.

| Question | Result |
|---|---|
| Hold model (Jones), N = 10³ … 10⁶, 4 distributions | heap faster on every seed at 16 of 16 points; heap/AVL throughput 1.48–2.13× |
| Hold model at N = 10⁶ | heap 4.67–4.87 M holds/s, AVL 2.46–3.26 M holds/s |
| Cancellation sweep, N = 10⁶, uniform victim, 0 → 95 % cancelled | heap 4.69 → 8.54 M steps/s, AVL 2.98 → 1.17 M; ratio 1.57 → 7.28×; no crossover |
| Small queues | the sorted array is fastest at N = 10 and 30, the heap from N = 100 |
| Heap size bound (D1) | peak physical / live entries 2.0000 at most |
| Simulator, 10⁶ events, 64.4 % of events cancelled | byte-identical output on heap, AVL and oracle |
| Predictions of the cost model (D6) | P2 and P7 confirmed; P1, P3–P6 refuted and explained (REPORT.md §11.7) |

## Known limitations

- The checkpoint tags `c1`, `c2` and `c3` were not created at the checkpoints; we have not created
  them afterwards. Each module arrived through one reviewed pull request (17–19 Nov) rather than in many
  small commits.
- The profiler was unavailable on the benchmark machine (macOS), so the report makes no cache claims;
  its cache explanations are labelled as untested hypotheses.
- All timings come from one machine and one benchmark run; a few points have a wide spread across seeds
  (e.g. exponential, N = 10⁶: ratio 1.70–2.64).
- The operation-count figure shows one seed per point (counting build), not five.
- The heap's `successor` is O(m) by design; the AVL tree's is O(log n).
- The default simulator keeps about 6·10⁴ events live; the 10⁶-resident requirement is met by the
  full-scale test, `make demo` and the benchmarks, not by the simulator's default run.
- The cost model (D6) was written after exploratory pilot runs (stated in its §0) and committed before
  the measurements; five of its seven predictions were refuted.
- The evaluation format for the hidden tests has not been published yet; `replay` follows INTERFACE.md §7.

## Tools and references

Every AI assistant and other tool we used, with what we changed and verified and representative
prompts, is declared in **TOOLS.md**. References are listed in REPORT.md §13; the main ones are
D. W. Jones, *An empirical comparison of priority-queue and event-set implementations*, CACM 29(4),
1986 (the hold model) and G. M. Adelson-Velsky and E. M. Landis, 1962 (AVL trees).

**How we worked.** Each module arrived through a pull request from its owner's account, reviewed by
another member (branch protection was not enabled, so this was by agreement; a few TOOLS.md edits were
committed directly to `main`). The submission is the commit tagged `final`.
