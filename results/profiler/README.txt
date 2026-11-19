STATUS: UNAVAILABLE
profiler: none
workload: bench_hold --dist exponential --n 1000000 --warmup 5000000; timed holds M=1000000 (heap, avl) and M=1 (heap, region self-check only)
counted region: the timed hold loop only (bench::ProfileRegion); per-hold figures = count / holds
region self-check: not run
NO USABLE PROFILER on this machine (needs perf with --control, or valgrind with a binary built against cachegrind.h >= 3.22): REPORT.md must make no cache claims
