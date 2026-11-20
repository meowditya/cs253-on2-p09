# TOOLS.md — every tool and AI used, on which parts, and what we changed

Declared use costs no marks (handout §2). Each member keeps their own rows accurate as they work.

## AI tools

| Date | Member | Tool | What it produced | What we changed afterwards |
|---|---|---|---|---|
| 2 Nov 2026 | Aditya Singh (Architect) | Claude (Anthropic) | the project design, a review of an external critique of that design, and PR #1: `include/event_queue.h`, `include/rng.h`, `INTERFACE.md`, `tests/interface_contract_test.cpp`, `Makefile`, README skeleton | *(Architect: list your edits)* |
| 3 Nov 2026 | Aditya Singh (Architect) | Claude (Anthropic) | a complete first draft of every module, generated in one session after INTERFACE.md was written: `src/structA/`, `src/structB/`, `oracle/`, `adversary/` (incl. suites), `app/`, `bench/`, `tests/`, `scripts/`, `results/plot.py`, drafts of `REPORT.md`, `docs/MODEL_D6.md` and `docs/derivations/D1–D6.md`. Claude compiled and tested the draft (g++ 13, clang 18 with libstdc++ and libc++, ASan/UBSan), ran short benchmark smoke runs, and ran the complete `make bench` matrix once, all on its own shared cloud VM and only to check the pipeline end to end (this is where README's "40 min" comes from); those numbers were discarded and none is used in REPORT.md. | see each owner's rows below |
| 9 Nov 2026 | Aditya Singh (Architect) | Claude (Anthropic) | checked an external code review (PROJECT9_FINAL_CODE_REVIEW.md) against the handout, reproduced each finding, and fixed: counters updated only in the counting build; checker probe-path reachability (+ mutant M8); `gen`, `bench_*`, `replay`, `difftest` argument validation and exception handling; overflow-safe sweep arithmetic; throughput columns (CSV v3) and a validating `plot.py`; profiling restricted to the timed loop; shrinker reports 1-minimality; full-scale test keeps exactly 10⁶ live; doc corrections (INTERFACE successor(INT64_MIN), D3, D5, D6 model §0/§2/§4, REPORT). | *(Architect: list what you checked / changed)* |
| 9 Nov 2026 | Aditya Singh (Architect) | Claude (Anthropic) | checked a second, follow-up review (PROJECT9_REVISED_ZIP_REVIEW.md), reproduced R1–R6 and fixed them: plot.py validates with one canonical workload key, rejects duplicates / missing mates / off-matrix settings / inconsistent values before writing anything, and labels exact ties (tests/plot_test.py); profile.sh reports USABLE / UNAVAILABLE / FAILED with exit status, rejects zero counts, and the benchmark checks perf's acknowledgements (tests/profile_test.sh with a fake perf); cachegrind region macros enabled only if they exist; CSV and stdout write failures exit 4; GNU `timeout` replaced by tests/run_timeout.py; run_bench.sh names any failed run. | *(Architect: list what you checked / changed)* |
| 20 Nov 2026 | Gulam Abbas (Structure A owner) | — | owner's review: `src/structA/` and `tests/test_structA.cpp` line by line | Reviewed Structure A (Lazy Heap) implementation and tests, including heap operations, lazy cancellation, hash table, and purge logic; no changes made. |
| 20 Nov 2026 | Aryan Arya (Structure B owner) | — | owner's review: `src/structB/` and `tests/test_structB.cpp` | Reviewed AVL queue implementation and associated tests, focusing on node pool management, AVL rotations, balancing logic and edge-case handling; no changes required.|
| *date* | Hiransh Anand (Oracle owner) | — | owner's review: `oracle/`, `tests/test_oracle.cpp`, `tests/test_checkers_mutations.cpp`, `tests/test_fullscale.cpp` | *(owner: add the date and what you checked or changed in your review; write "no changes" if none)* |
| *date* | Sanchit Goyal (Adversary) | — | owner's review: `adversary/`, `scripts/run_suites.sh` | *(owner: add the date and what you checked or changed in your review; write "no changes" if none)* |
| *date* | Ram Upadhyay (Analyst) | — | owner's review: `bench/`, `results/plot.py`, `scripts/run_bench.sh`, `scripts/profile.sh`, revised `docs/MODEL_D6.md` | *(owner: add the date and what you checked or changed in your review; write "no changes" if none)* |
| 13 Nov 2026 | Aditya Singh (Architect) | Claude (Anthropic) | filled in the team roles; wrote REPORT.md sections 2, 3, 4 (closing paragraph), 7 (explanations) and 8; wrote `scripts/fill_report.py`, which copies the measured results of `make bench` into REPORT.md §6 and marks each prediction confirmed or refuted mechanically; exported the Claude-drafted derivations `docs/derivations/D1–D6.md` to `D1–D6.pdf` with each owner's name (`scripts/make_pdfs.sh`). No measurement was produced by Claude: §6 is filled only by the team's own `make bench` run. | *(each author: note here if you rewrote or corrected your derivation)* |
| 19 Nov 2026 | Aditya Singh (Architect) | — | owner's review: `app/`, `tests/test_sim.cpp`, `docs/derivations/D3.md` | checked and verified overall |
| 19 Nov 2026 | Aditya Singh (Architect) | Claude (Anthropic) | checked the `make bench` results: regenerated the figures, summaries and REPORT §6 from the raw CSVs and re-derived the P1–P7 verdicts; updated slide 11 and the study notes. No measurement was produced by Claude. | none to the results |
| 17–19 Nov 2026 | whole team | Git / GitHub | the reviewed tree was pushed module by module through pull requests, each from its owner's account | files copied unchanged from `Project9_complete.zip` |
| 19 Nov 2026 | Sanchit Goyal (Adversary) | OpenAI Codex | Imported the supplied adversary generators, distributions, 16 suites and expected outputs, suite runner, CLI regression tests, timeout helper, and D5 Markdown/PDF into `adversary/generators`; prepared Git commit and validation. | Source files copied unchanged from `Project9_complete.zip`; updated this disclosure. Validation results and repository limitations are recorded below. |

## Other tools

| Tool | Version | Used for |
|---|---|---|
| g++ / clang++ | Apple clang 21.0.0 (benchmark run, Aditya's Mac); g++ 15.2.0 (Sanchit's validation) | compiling (C++17, `-Wall -Wextra -pedantic -Werror`) |
| GNU make | | build |
| AddressSanitizer, UndefinedBehaviorSanitizer | (bundled with the compiler) | `make sanitize` |
| Python 3 + matplotlib | | `results/plot.py` (figures only) |
| perf or valgrind (cachegrind) | | cache evidence, if available (`scripts/profile.sh`) |
| Git / GitHub | | version control, pull requests, reviews |
| Overleaf / LaTeX (or Markdown → PDF) | | derivation PDFs |

## Adversary import validation — 19 Nov 2026

Codex imported the requested files for Sanchit Goyal without changing the supplied
source, fixtures, or D5 documents. The shell scripts and Python timeout helper
were marked executable for Linux. No personal owner-review claim was filled in.

Validation used a separate extraction of the complete supplied ZIP, with g++
15.2.0 and Python 3.14.4. The `gen`, `replay`, `bench_hold`, `bench_sweep`, and
`difftest` release binaries built with the supplied Makefile.

- All 16 suites matched their expected output on heap, AVL, and oracle (48 replays).
- Attack checks passed: bloat 3100 physical entries without purge versus 200
  with purge (100 live); drain 500 versus 1; ascending 990/1000 rebalances.
- `tests/cli_test.sh` passed all executed checks. Its plot.py check was skipped
  because matplotlib was not installed.
- Shell/Python syntax checks passed; all 16 suites have expected-output mates.
- All 39 imported source, fixture, and D5 files match the ZIP byte for byte.

At import time, remote `main` contained only the empty initial commit
`7a8c44cc6f963247345757c2c1a3ce8fc5d884d7`. This branch adds only the files
requested for the adversary contribution, plus this disclosure. It needs the
other project modules and build files before these checks can run from the
repository itself; the validation above used the complete ZIP's dependencies.
