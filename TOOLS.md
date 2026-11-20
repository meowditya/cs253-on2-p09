# Tools used

Every AI assistant, code generator and other significant tool we used (submission guidelines §6).
Declared use costs no marks. Each member keeps their own rows accurate.

## AI tools and reviews

| Tool and version | Used by | Used for | Files affected | What we changed or verified |
|---|---|---|---|---|
| Claude (Anthropic, claude.ai; model version not recorded), 2 Nov 2026 | Aditya Singh (Architect) | the project design, a review of an external critique of that design, and PR #1: `include/event_queue.h`, `include/rng.h`, `INTERFACE.md`, `tests/interface_contract_test.cpp`, `Makefile`, README skeleton | `include/event_queue.h`, `include/rng.h`, `INTERFACE.md`, `tests/interface_contract_test.cpp`, `Makefile`, `README.md` | reviewed the generated interface; committed it on 2 Nov, then committed the final version after the 9 Nov review fixes (PR #1, 17 Nov) and the shared test helper (PR #2) |
| Claude (Anthropic, claude.ai; model version not recorded), 3 Nov 2026 | Aditya Singh (Architect) | a complete first draft of every module, generated in one session after INTERFACE.md was written: `src/structA/`, `src/structB/`, `oracle/`, `adversary/` (incl. suites), `app/`, `bench/`, `tests/`, `scripts/`, `results/plot.py`, drafts of `REPORT.md`, `docs/MODEL_D6.md` and `docs/derivations/D1–D6.md`. Claude compiled and tested the draft (g++ 13, clang 18 with libstdc++ and libc++, ASan/UBSan), ran short benchmark smoke runs, and ran the complete `make bench` matrix once, all on its own shared cloud VM and only to check the pipeline end to end; those numbers were discarded and none is used in REPORT.md. | every module: `src/structA/`, `src/structB/`, `oracle/`, `adversary/`, `app/`, `bench/`, `tests/`, `scripts/`, `results/plot.py`, `REPORT.md`, `docs/MODEL_D6.md`, `docs/derivations/D1–D6.md` | see each owner's rows below |
| Claude (Anthropic, claude.ai; model version not recorded), 9 Nov 2026 | Aditya Singh (Architect) | checked an external code review (PROJECT9_FINAL_CODE_REVIEW.md) against the handout, reproduced each finding, and fixed: counters updated only in the counting build; checker probe-path reachability (+ mutant M8); `gen`, `bench_*`, `replay`, `difftest` argument validation and exception handling; overflow-safe sweep arithmetic; throughput columns (CSV v3) and a validating `plot.py`; profiling restricted to the timed loop; shrinker reports 1-minimality; full-scale test keeps exactly 10⁶ live; doc corrections (INTERFACE successor(INT64_MIN), D3, D5, D6 model §0/§2/§4, REPORT). | `include/event_queue.h`, `oracle/checkers.cpp`, `tests/test_checkers_mutations.cpp`, `adversary/gen.cpp`, `bench/`, `oracle/replay_main.cpp`, `oracle/difftest_*`, `results/plot.py`, `scripts/profile.sh`, `tests/test_fullscale.cpp`, `INTERFACE.md`, `docs/derivations/D3.md`, `D5.md`, `docs/MODEL_D6.md`, `REPORT.md` | obtained the external review that found these defects and gave it to Claude; checked the fixes and committed them (PR #1, 17 Nov; PR #9, 19 Nov); reran make test on my Mac on 20 Nov, all passed |
| Claude (Anthropic, claude.ai; model version not recorded), 9 Nov 2026 | Aditya Singh (Architect) | checked a second, follow-up review (PROJECT9_REVISED_ZIP_REVIEW.md), reproduced R1–R6 and fixed them: plot.py validates with one canonical workload key, rejects duplicates / missing mates / off-matrix settings / inconsistent values before writing anything, and labels exact ties (tests/plot_test.py); profile.sh reports USABLE / UNAVAILABLE / FAILED with exit status, rejects zero counts, and the benchmark checks perf's acknowledgements (tests/profile_test.sh with a fake perf); cachegrind region macros enabled only if they exist; CSV and stdout write failures exit 4; GNU `timeout` replaced by tests/run_timeout.py; run_bench.sh names any failed run. | `results/plot.py`, `tests/plot_test.py`, `scripts/profile.sh`, `tests/profile_test.sh`, `bench/`, `scripts/run_bench.sh`, `tests/run_timeout.py` | obtained the external review that found these defects and gave it to Claude; checked the fixes and committed them (PR #1, 17 Nov; PR #9, 19 Nov); reran make test on my Mac on 20 Nov, all passed |
| Claude (Anthropic, claude.ai; model version not recorded), 13 Nov 2026 | Aditya Singh (Architect) | filled in the team roles; wrote REPORT.md sections 2, 3, 4 (closing paragraph), 7 (explanations) and 8; wrote `scripts/fill_report.py`, which copies the measured results of `make bench` into REPORT.md §6 and marks each prediction confirmed or refuted mechanically; exported the Claude-drafted derivations `docs/derivations/D1–D6.md` to `D1–D6.pdf` with each owner's name (`scripts/make_pdfs.sh`). No measurement was produced by Claude: §6 is filled only by the team's own `make bench` run. | `README.md`, `REPORT.md`, `scripts/fill_report.py`, `scripts/make_pdfs.sh`, `docs/derivations/D1–D6.pdf` | D3: not rewritten; D3.pdf re-committed in PR #12 after the Markdown had been committed in its place. Other derivations: no changes reported by 20 Nov |
| OpenAI Codex, 19 Nov 2026 | Sanchit Goyal (Adversary) | Imported the supplied adversary generators, distributions, 16 suites and expected outputs, suite runner, CLI regression tests, timeout helper, and D5 Markdown/PDF into `adversary/generators`; prepared Git commit and validation. | `adversary/`, `scripts/run_suites.sh`, `tests/cli_test.sh`, `tests/run_timeout.py`, `docs/derivations/D5.md`, `D5.pdf`, `TOOLS.md` | Source files copied unchanged from `Project9_complete.zip`; updated this disclosure. Validation results and repository limitations are recorded below. |
| Claude (Anthropic, claude.ai; model version not recorded), 19 Nov 2026 | Aditya Singh (Architect) | checked the `make bench` results: regenerated the figures, summaries and REPORT §6 from the raw CSVs and re-derived the P1–P7 verdicts; updated the analyst slides and the study notes. No measurement was produced by Claude. | `results/figures/`, `results/summary.csv`, `REPORT.md` §6 (now §10–11), slides, study notes | none to the results |
| none — owner's review, 19 Nov 2026 | Aditya Singh (Architect) | owner's review: `app/`, `tests/test_sim.cpp`, `docs/derivations/D3.md` | `app/`, `tests/test_sim.cpp`, `docs/derivations/D3.md` | checked and verified overall |
| Git / GitHub, 17–19 Nov 2026 | whole team | the reviewed tree was pushed module by module through pull requests, each from its owner's account | the whole tree | files copied unchanged from `Project9_complete.zip` |
| none — owner's review, 20 Nov 2026 | Gulam Abbas (Structure A owner) | owner's review: `src/structA/` and `tests/test_structA.cpp` line by line | `src/structA/`, `tests/test_structA.cpp` | Reviewed Structure A (Lazy Heap) implementation and tests, including heap operations, lazy cancellation, hash table, and purge logic; no changes made. |
| none — owner's review, 20 Nov 2026 | Aryan Arya (Structure B owner) | owner's review: `src/structB/` and `tests/test_structB.cpp` | `src/structB/`, `tests/test_structB.cpp` | Reviewed AVL queue implementation and associated tests, focusing on node pool management, AVL rotations, balancing logic and edge-case handling; no changes required |
| none — owner's review, 20 Nov 2026 | Hiransh Anand (Oracle owner) | owner's review: `oracle/`, `tests/test_oracle.cpp`, `tests/test_checkers_mutations.cpp`, `tests/test_fullscale.cpp` | `oracle/`, `tests/test_oracle.cpp`, `tests/test_checkers_mutations.cpp`, `tests/test_fullscale.cpp` | Implement a sorted-array event queue with binary-search insertion, and prove its checker validates BST order, stored heights, and AVL balance exactly. |
| OpenAI Codex (Codex-assisted owner's review), 20 Nov 2026 | Sanchit Goyal (Adversary) | owner's review: `adversary/`, `scripts/run_suites.sh` | `adversary/`, `scripts/run_suites.sh` | Codex-assisted review of adversarial generators, distributions and suite runner; all 16 suites passed on heap, AVL and oracle using the supplied ZIP; no source changes. |
| none — owner's review, 20 Nov 2026 | Ram Upadhyay (Analyst) | owner's review: `bench/`, `results/plot.py`, `scripts/run_bench.sh`, `scripts/profile.sh`, revised `docs/MODEL_D6.md` | `bench/`, `results/plot.py`, `scripts/run_bench.sh`, `scripts/profile.sh`, `docs/MODEL_D6.md` | all verified |
| Claude (Anthropic, claude.ai; model version not recorded), 20 Nov 2026 | Aditya Singh (Architect) | the presentation deck (slides, figures and speaker notes, including the two results charts drawn from `results/summary.csv`) and study notes for Aditya, Aryan and Sanchit | slides (`docs/slides/slides.pdf`), study notes (not in the repository) | directed the design and structure; caught and had fixed: animations removed in a redraft, a logic error on the problem slide (a timeout cancelled before its ACK had run, a lost packet's flow missing), missing spacing in the findings and references; asked for the results plots and the slide merge; the measured numbers are only those in REPORT.md |
| Claude Opus 5.5 (Anthropic, claude.ai), 21 Nov 2026 | Aditya Singh (Architect) | checked the repository against the course's *Lab Project Submission Guidelines* and prepared the changes: README restructured into the required sections (roles table with roll numbers and GitHub usernames, quick start with expected output, evaluation interface); REPORT.md reordered into the 13 required sections, adding team contributions with PR links, the bug log, the requirement beyond the lecture (Jones's hold model), the predicted-model commit links, a caption naming the script and raw file for every figure, an explanation of every refuted prediction, limitations and references; `REPORT.pdf`; derivations renamed `D<k>_<roll no>.pdf` with roll numbers and a "Where it is used" section; `scripts/run_all.sh`; `replay` also reads stdin; `plot.py` draws the P1 predictions and the P3 range on the same axes as the measurements (new `ratio_vs_predicted.png`); this file reformatted with representative prompts; the slides exported to `docs/slides/slides.pdf`; a follow-up after the final audit: D6's roll number on its first page, the report's links pointed at the renamed repository, a stale file name in INTERFACE.md | `README.md`, `REPORT.md`, `REPORT.pdf`, `TOOLS.md`, `docs/derivations/`, `docs/slides/slides.pdf`, `scripts/run_all.sh`, `scripts/make_pdfs.sh`, `scripts/fill_report.py`, `results/plot.py`, `results/figures/counts_per_hold.png`, `results/figures/ratio_vs_predicted.png`, `oracle/replay_main.cpp` | Aditya supplied the roll numbers and GitHub usernames and committed the changes from his account. Claude reran `make test` on the changed tree (Ubuntu 24.04, g++ 13.3.0): all tests passed. No measurement changed: every number and figure is regenerated from the committed `results/raw/*.csv`; the cost model `docs/MODEL_D6.md` is untouched. |

## Other tools

| Tool and version | Used by | Used for | Files affected | What we changed or verified |
|---|---|---|---|---|
| Apple clang 21.0.0 (clang-2100.4.1.1) | Aditya Singh | compiling; the benchmark run of 19 Nov | the whole tree (build only) | — |
| g++ 13.3.0, g++ 15.2.0, clang 18 (libstdc++ and libc++) | Claude (VM checks); Sanchit Goyal (g++ 15.2.0 validation) | compiling with `-std=c++17 -Wall -Wextra -pedantic -Werror`; identical-output checks across toolchains | the whole tree (build only) | logs in `docs/verification/` |
| AddressSanitizer, UndefinedBehaviorSanitizer (bundled with the compiler) | whole team | `make sanitize` | — | clean over the whole test suite |
| GNU make | whole team | build | `Makefile` | — |
| Python 3 + matplotlib | Ram Upadhyay, Aditya Singh | CSV validation and figures (`results/plot.py`), report tables (`scripts/fill_report.py`), test scripts | `results/` | — |
| perf / valgrind (cachegrind) | Ram Upadhyay | cache evidence if available (`scripts/profile.sh`) | `results/profiler/` | unavailable on the benchmark Mac: status UNAVAILABLE, no cache claims |
| pandoc + XeLaTeX (DejaVu fonts) | Aditya Singh | Markdown → PDF for the derivations and the report (`scripts/make_pdfs.sh`) | `docs/derivations/*.pdf`, `REPORT.pdf` | — |

## Representative prompts

**Claude — Aditya Singh** (quoted as typed):

- 19 Nov 2026: "check each file in github now and tell me how to finalise it"
- 20 Nov 2026: "i want you to enhace the ppt, make it more like a formal research paper style while still keeping the live illustrations for reference, make the speaker notes more professional and make it so that it is actually what we're speaking"
- 20 Nov 2026: "on slide 2, f1 f2 f3 are not all there, also when we pop_min, only it's timeout gets cancelled so fix this"
- 20 Nov 2026: "remove drain test and bloat test from page 6 on the slides, abbas is telling me that it is creating confusion"
- 21 Nov 2026: "check if github repo is finalised acc to these guidelines / tell me if i have to make any changes"

The prompts of the 2–13 Nov sessions (design, first draft, the two review rounds) are in Aditya's
Claude chat history and can be shown on request.

**OpenAI Codex — Sanchit Goyal** (19 Nov 2026 import, 20 Nov review): the prompts are in Sanchit's
Codex history and can be shown on request.

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
