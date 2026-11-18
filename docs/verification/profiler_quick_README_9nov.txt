STATUS: USABLE
profiler: cachegrind
workload: bench_hold --dist exponential --n 100000 --warmup 200000; timed holds M=200000 (heap, avl) and M=1 (heap, region self-check only)
counted region: the timed hold loop only (bench::ProfileRegion); per-hold figures = count / holds
region self-check: PASSED (1 timed hold counted 5141 instructions, > 0 and < 100 000; init + warm-up are excluded)
evidence: summary.csv and one raw output file per run
NOTE: cachegrind SIMULATES a cache (its own L1/LL model); it is not a hardware measurement. Quote it as simulated.
