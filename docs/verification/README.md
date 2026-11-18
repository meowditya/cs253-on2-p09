# Verification logs (9 Nov 2026, Claude's cloud VM — not the team's benchmark machine)

Produced after the fixes for both external reviews, on fresh copies of this tree:

| Log | What ran |
|---|---|
| `gcc_9nov.log` | g++ 13 / libstdc++: `make test`, then `make sanitize` (ASan + UBSan, leak detection ON) |
| `clang_9nov.log` | clang 18 / libstdc++: `make test` |
| `libcxx_9nov.log` | clang 18 / libc++: `make test` |
| `summary_9nov.txt` | exit statuses and the simulator's golden lines on each toolchain |
| `bench_quick_9nov.log` | `make bench-quick`: every run, plot.py validation of the real quick CSVs, cachegrind profile |
| `profiler_quick_*_9nov.*` | that profile's README (STATUS USABLE, region self-check PASSED) and summary — cachegrind, i.e. a SIMULATED cache |

Each `make test` includes `tests/cli_test.sh`, `tests/profile_test.sh` (fake perf: logic only) and
`tests/plot_test.py` (synthetic fixture: validation logic only). These logs show the pipeline works;
their timings are NOT measurements for REPORT.md. Re-run `make test` and `make sanitize` on your own
machine before the `final` tag and replace these logs. The real `perf` path was not available here.

13 Nov: only documents and scripts changed after the logs above (roles, report text, `fill_report.py`,
`make_pdfs.sh`, machine description); `gcc_13nov_make_test.log` is `make test` on that final tree.
