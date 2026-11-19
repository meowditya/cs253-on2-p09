# Project 9 — Event scheduler: heap against balanced tree

CS 253 / MA 253 Data Structures and Algorithms Laboratory, IIT Indore · Team **O(n²)**

A network simulator keeps a *future event set*: timestamped events — ACK arrivals, retransmission
timeouts, watchdog timers — most of which are cancelled before they fire. We implement that event set
twice and compare them:

- **Structure A** — a binary min-heap keyed by (time, seq) with **lazy cancellation** recorded in a hash
  table written from scratch, purged whenever cancelled entries outnumber live ones.
- **Structure B** — an **AVL tree** keyed by (time, seq) used as a priority queue, with true deletion and
  `successor(t)`.

Both are checked against a sorted-array **oracle** by a differential tester, attacked with adversarial
inputs, driven by a retransmission-timer **simulator** that must print identical output on either
structure, and measured under **Jones's hold model** and a **cancellation sweep**.

## One command

```bash
make all
```

builds everything, runs every correctness test, the full-scale demo, and every benchmark (CSV →
figures), strictly one after another. Requirements: Linux, WSL or macOS; `g++` or `clang++` with C++17;
GNU make; Python 3 (the test scripts use it; matplotlib for figures and the CSV-validation test). No
GNU coreutils-only tools are needed. Optional for cache evidence: Linux `perf` with `--control`
support, or `valgrind` ≥ 3.22 (cachegrind, a simulated cache); see `scripts/profile.sh`, which ends with
STATUS USABLE, UNAVAILABLE (no profiler: no cache claims) or FAILED (`make bench` then exits 1 after
writing the valid timing results).

| Target | What it does | Time on our test machine |
|---|---|---|
| `make` | builds all programs and tests into `build/release/bin/` | < 1 min |
| `make test` | unit tests, 8 mutation tests, command-line hardening, profiler-logic and CSV-validation checks, 16 adversarial/edge suites on 3 queues, three 10⁶-operation differential histories, a 10⁶-resident full-scale test, simulator identical-output checks | ~1 min |
| `make demo` | the live demonstration (see below) | ~15 s |
| `make bench` | the full benchmark matrix → `results/raw/*.csv`, then `plot.py` validates every CSV and writes `results/summary.csv`, `results/paired_ratios.csv`, throughput figures `results/figures/*.png`; `scripts/fill_report.py` then writes REPORT.md §6.1 and §6.3–6.8 from those files; cache profile of the timed loop only → `results/profiler/`  about 13 min on an Apple M4 (19 Nov run) |
| `make bench-quick` | a ~1-minute smoke version of every experiment (not for the report) | ~1 min |
| `make pdfs` | rebuild `docs/derivations/D1–D6.pdf` from their sources (needs pandoc + xelatex; the PDFs are committed) | seconds |
| `make sanitize` | the tests rebuilt with AddressSanitizer + UndefinedBehaviorSanitizer; leak detection on Linux (Apple's toolchain has no LeakSanitizer, so it is off on macOS — the run prints which) | ~3 min |
| `make BUILD=count …` | operation-counting build (`-DEQ_COUNTING=1`) — never time it | |

## Programs (`build/release/bin/`)

| Program | Use |
|---|---|
| `sim` | the application. `sim --queue heap\|avl\|oracle [--events E] [--flows F] [--loss-ppm P] [--seed S] [--rtt-min US] [--rtt-max US] [--rto US] [--idle US] [--monitor US] [--trace]` |
| `replay` | run an operation stream: `replay --queue heap\|avl\|oracle [--check N] [--stats] file.ops` (format in INTERFACE.md §7) |
| `difftest` | differential test: `difftest --seed S --ops N [--max-live L]`; on failure it shrinks and saves `failures/seed<S>.ops` |
| `gen` | adversarial streams: `gen ascending N`, `descending N`, `zigzag N`, `fib-delete H D`, `bloat L STEPS`, `drain N`, `storm N`, `regrow N`, `latesucc N Q`, `random SEED OPS LIVE` |
| `bench_hold` | hold model: `bench_hold --queue heap\|avl\|oracle\|control --dist exponential\|uniform\|bimodal\|triangular\|biased --n N` |
| `bench_sweep` | cancellation sweep: `bench_sweep --queue heap\|avl --cancel-bp 0..10000 --policy uniform\|latest --n N` |

Exit codes everywhere: 0 ok · 2 bad arguments/input · 3 a check failed · 4 resource/overflow, including
a result or `--csv` row that could not be written. `bench_hold --profile-support` prints what the binary
supports for profiling.

## Golden values (check them on your machine)

`build/release/bin/sim` with default parameters must end with

```
checksum=0xcbd12e722cac3922
monitor_successors=0xebce99f1c00b6707
```

on every queue. We obtained these identical values with g++ 13 + libstdc++, clang 18 + libstdc++ and
clang 18 + libc++ (the macOS standard library). Any correct build on any machine must reproduce them;
if yours differs, the build is not equivalent — report it.

## The live demonstration (`make demo`, from a fresh clone of the `final` tag)

1. the simulator, 10⁶ dispatched events, on the heap — canonical summary printed;
2. the same run on the AVL tree — `cmp` shows byte-identical output;
3. seeds 1–3 traced event by event on heap, AVL and oracle, all identical;
4. 10⁶ events resident at once in each structure (`--stats`);
5. the heap-bloat attack with and without purging.

## Team and roles (fixed at the first checkpoint)

| Role | Member | Owns | Derivation |
|---|---|---|---|
| Architect | Aditya Singh (250003007) | `include/`, `app/`, `scripts/` (except bench/suite scripts), `Makefile`, `INTERFACE.md`, this README | D3 |
| Structure A owner | Gulam Abbas | `src/structA/` | D1 |
| Structure B owner | Aryan Arya | `src/structB/` | D2 |
| Oracle owner | Hiransh Anand | `oracle/`, `tests/test_fullscale.cpp`, `tests/test_checkers_mutations.cpp` | D4 |
| Adversary | Sanchit Goyal | `adversary/`, `scripts/run_suites.sh` | D5 |
| Analyst | Ram Upadhyay | `bench/`, `results/`, `scripts/run_bench.sh`, `scripts/profile.sh`, `docs/MODEL_D6.md` | D6 |

Unit tests live with their module's owner (`tests/test_structA.cpp` → Structure A owner, etc.).

## Repository layout

```
include/        event_queue.h (interface) · rng.h · canonical.h (hashing) · opstream.h (.ops format)
src/structA/    seq_table.{h,cpp} (hash table) · lazy_heap.{h,cpp}
src/structB/    avl_queue.{h,cpp}
oracle/         sorted_oracle · checkers · replay_core · difftest_core · replay_main · difftest_main
adversary/      distributions.h (Jones Table I) · gen.cpp · suites/*.ops + *.expected
app/            simulator.{h,cpp} · sim_main.cpp · queue_factory.{h,cpp}
bench/          hold.cpp · cancel_sweep.cpp · bench_common.h
tests/          unit, mutation, simulator and full-scale tests
scripts/        run_tests.sh · run_suites.sh · check_identical.sh · demo.sh · run_bench.sh · profile.sh
results/        raw/ (CSV) · figures/ · profiler/ · plot.py · machine.txt · summary.csv (after make bench)
docs/           MODEL_D6.md · derivations/D1..D6 (.md drafts → PDFs named by author)
INTERFACE.md  REPORT.md  TOOLS.md
```

## Rules we follow

- `main` is protected: every change arrives through a pull request approved by another member.
- Each member commits their own module from their own account, in their own directory.
- No library container stands in for a structure under study: the heap, its hash table, the AVL tree
  and the oracle's binary search are ours. `std::vector` is used only as raw storage; `std::set` /
  `std::unordered_map` appear only in test-data and benchmark-stream generation.
- Every AI or other tool is declared in TOOLS.md.
- The submission is the commit tagged `final`.
