#!/usr/bin/env bash
# scripts/run_bench.sh — the whole benchmark matrix. Owner: Analyst. Called by `make bench`.
#
#   run_bench.sh --full    the matrix for REPORT.md (5 seeds per point; takes a while)
#   run_bench.sh --quick   a few-minute smoke run of every experiment (NOT for the report)
#
# Writes results/raw/{hold,sweep}.csv (timing, release build) and *_count.csv (operation counts,
# count build, untimed meaning), results/machine.txt, results/profiler/, then validates the CSVs and
# draws throughput figures via results/plot.py --mode full|quick (the matrix there must match this one).
# A and B always get the same seed/stream; the order alternates (AB for odd seeds, BA for even).
set -euo pipefail
export LC_ALL=C
# Any failed benchmark run stops everything and names the exact command (no silently missing rows):
trap 'echo "run_bench: FAILED (exit $?) at line $LINENO: $BASH_COMMAND" >&2' ERR
BIN=${BIN:-build/release/bin}
COUNT_BIN=${COUNT_BIN:-build/count/bin}
MODE=${1:---full}
[[ $MODE == --full || $MODE == --quick ]] || { echo "usage: run_bench.sh --full|--quick" >&2; exit 2; }
RAW=results/raw
mkdir -p "$RAW" results/figures results/profiler
stamp=$(date +%Y%m%d-%H%M%S)
for f in hold.csv sweep.csv hold_count.csv sweep_count.csv; do
  [[ -f "$RAW/$f" ]] && mv "$RAW/$f" "$RAW/${f%.csv}.$stamp.old.csv"     # never mix two runs
done

# ---- the machine (required: one stated machine for every number) ----
{
  echo "date: $(date)"
  echo "uname: $(uname -a)"
  if command -v lscpu >/dev/null; then lscpu | grep -E 'Model name|^CPU\(s\)|L1d|L2|L3|MHz'
  elif command -v sysctl >/dev/null; then sysctl -n machdep.cpu.brand_string 2>/dev/null; sysctl hw.ncpu hw.l1dcachesize hw.l2cachesize hw.l3cachesize hw.memsize 2>/dev/null; fi
  echo "compiler: $(${CXX:-g++} --version | head -1)"
  echo "flags: -std=c++17 -O2 -DNDEBUG (release); counting build adds -DEQ_COUNTING=1"
  echo "mode: $MODE"
  if command -v pmset >/dev/null; then echo "power: $(pmset -g batt | head -1)"
  elif [[ -r /sys/class/power_supply/AC/online ]]; then echo "power: AC online = $(cat /sys/class/power_supply/AC/online)"
  elif [[ -r /sys/class/power_supply/ACAD/online ]]; then echo "power: AC online = $(cat /sys/class/power_supply/ACAD/online)"
  else echo "power: not detected (desktop or VM)"; fi
  echo "load average at start: $(uptime | sed 's/.*load average[s]*: *//')"
  echo "run conditions: one run, benchmarks strictly one after another; close other programs and stay plugged in"
} > results/machine.txt

if [[ $MODE == --quick ]]; then
  NS="1000 10000 100000"; SEEDS="1 2"; HOLDS=200000; WARM=200000
  SMALL_NS="10 100"; SWEEP_N=100000; SWEEP_STEPS=200000; CS="0 5000 9500"; SCALE_NS=""; SENS_N=10000
else
  NS="1000 10000 100000 1000000"; SEEDS="1 2 3 4 5"; HOLDS=""; WARM=""
  SMALL_NS="10 30 100 300"; SWEEP_N=1000000; SWEEP_STEPS=1000000
  CS="0 1000 2000 3000 4000 5000 6000 7000 8000 9000 9500"; SCALE_NS="10000 100000"; SENS_N=100000
fi
DISTS="exponential uniform bimodal triangular"
hold_args() { [[ -n $HOLDS ]] && echo "--holds $HOLDS --warmup $WARM" || true; }
pair() { if (( $1 % 2 == 1 )); then echo "heap avl"; else echo "avl heap"; fi; }

echo "== hold model: N in {$NS} x {$DISTS} x seeds {$SEEDS}"
for n in $NS; do for d in $DISTS; do for s in $SEEDS; do
  i=0; for q in $(pair "$s"); do
    "$BIN/bench_hold" --queue "$q" --dist "$d" --n "$n" --seed "$s" --order "$i" $(hold_args) --csv "$RAW/hold.csv" > /dev/null
    i=$((i + 1))
  done
done; done; echo "   N=$n done"; done
echo "== overhead control (scripted replay, no queue)"
for n in $NS; do for s in $SEEDS; do
  "$BIN/bench_hold" --queue control --dist exponential --n "$n" --seed "$s" $(hold_args) --csv "$RAW/hold.csv" > /dev/null
done; done
echo "== small-N crossover: heap, avl, oracle at N in {$SMALL_NS}"
for n in $SMALL_NS; do for s in $SEEDS; do for q in heap avl oracle; do
  "$BIN/bench_hold" --queue "$q" --dist exponential --n "$n" --seed "$s" --holds 1000000 --warmup 100000 --csv "$RAW/hold.csv" > /dev/null
done; done; done
echo "== quantisation sensitivity: mean 10^6 us instead of 10^3 us (N=$SENS_N)"
for d in exponential bimodal; do for s in $SEEDS; do for q in $(pair "$s"); do
  "$BIN/bench_hold" --queue "$q" --dist "$d" --n "$SENS_N" --seed "$s" --mean-us 1000000 $(hold_args) --csv "$RAW/hold.csv" > /dev/null
done; done; done

echo "== cancellation sweep: N=$SWEEP_N, policy uniform, c in {$CS} (basis points)"
for c in $CS; do for s in $SEEDS; do i=0; for q in $(pair "$s"); do
  "$BIN/bench_sweep" --queue "$q" --policy uniform --cancel-bp "$c" --n "$SWEEP_N" --steps "$SWEEP_STEPS" --seed "$s" --order "$i" --csv "$RAW/sweep.csv" > /dev/null
  i=$((i + 1))
done; done; echo "   c=$c done"; done
echo "== stress policy (latest key) and scaling, c in {0, 5000, 9500}"
for n in $SWEEP_N $SCALE_NS; do for p in latest uniform; do
  [[ $p == uniform && $n == "$SWEEP_N" ]] && continue
  for c in 0 5000 9500; do for s in $SEEDS; do for q in $(pair "$s"); do
    "$BIN/bench_sweep" --queue "$q" --policy "$p" --cancel-bp "$c" --n "$n" --steps "$SWEEP_STEPS" --seed "$s" --csv "$RAW/sweep.csv" > /dev/null
  done; done; done
done; done

echo "== operation counts (count build, one seed per point)"
for n in $NS; do for d in $DISTS; do for q in heap avl; do
  "$COUNT_BIN/bench_hold" --queue "$q" --dist "$d" --n "$n" --seed 1 $(hold_args) --csv "$RAW/hold_count.csv" > /dev/null
done; done; done
for c in $CS; do for q in heap avl; do
  "$COUNT_BIN/bench_sweep" --queue "$q" --policy uniform --cancel-bp "$c" --n "$SWEEP_N" --steps "$SWEEP_STEPS" --seed 1 --csv "$RAW/sweep_count.csv" > /dev/null
done; done

echo "== figures (plot.py validates every CSV first)"
python3 results/plot.py --mode "${MODE#--}"

echo "== profiler (timed region only; optional)"
prof=0
BIN="$BIN" scripts/profile.sh "$MODE" || prof=$?
python3 scripts/fill_report.py --mode "${MODE#--}"
echo "benchmarks done: results/raw/*.csv, results/summary.csv, results/paired_ratios.csv, results/figures/*.png"
case $prof in
  0) echo "profiler: USABLE — results/profiler/summary.csv" ;;
  5) echo "profiler: UNAVAILABLE on this machine — REPORT.md makes no cache claims" ;;
  *) echo "########## PROFILING FAILED (status $prof) — see results/profiler/README.txt ##########" >&2
     echo "The timing CSVs and figures above are valid; the cache numbers are NOT. Exiting 1." >&2
     exit 1 ;;
esac
