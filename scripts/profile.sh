#!/usr/bin/env bash
# scripts/profile.sh — cache evidence for REPORT.md, restricted to the TIMED holds. Owner: Analyst.
#
#   scripts/profile.sh [--full|--quick]        (EQ_PROFILER=auto|perf|cachegrind|none to force a tool)
#
# Exit status and results/profiler/STATUS:
#   0  USABLE       a profiler ran and every check below passed
#   5  UNAVAILABLE  no usable profiler on this machine (not an error: REPORT.md makes no cache claims)
#   1  FAILED       a profiler ran but a run or a check failed — its numbers must NOT be used
#   2  bad usage
#
# The benchmark brackets its timed loop with bench::ProfileRegion (bench/bench_common.h), so the
# profiler counts only the timed holds — not the Jones initialisation or the warm-up:
#   * perf (hardware counters, Linux): `perf stat --control fifo:CTL,ACK --delay=-1`; counters start
#     disabled and the benchmark sends "enable"/"disable" through the FIFOs and waits for perf's "ack".
#     If any command is not acknowledged the benchmark exits 3, and this script reports FAILED.
#   * cachegrind (a SIMULATED cache, not hardware): `--instr-at-start=no`; the benchmark switches
#     instrumentation on/off with Valgrind client requests. Used only if the BINARY was built with them
#     (`bench_hold --profile-support` says cachegrind_regions=yes).
# Checks (all must pass for USABLE): every benchmark run exits 0; every instruction count is a positive
# integer; the region self-check — the heap run with ONE timed hold counts more than 0 and fewer than
# 100 000 instructions (the untimed initialisation + warm-up execute hundreds of millions). That
# threshold is a sanity check that the bracketing works, not a proof of its exact scope. Per-hold
# figures are count / M; nothing is subtracted.
set -uo pipefail
export LC_ALL=C
BIN=${BIN:-build/release/bin}
OUT=${PROFILE_OUT:-results/profiler}
MODE=${1:---full}
[[ $MODE == --full || $MODE == --quick ]] || { echo "usage: profile.sh [--full|--quick]" >&2; exit 2; }
mkdir -p "$OUT"
rm -f "$OUT"/*.txt "$OUT"/*.stderr "$OUT"/*.csv "$OUT"/STATUS
if [[ $MODE == --quick ]]; then N=100000; M=200000; else N=1000000; M=1000000; fi
W=$((5 * N))
note=""

run_perf() {                      # run_perf QUEUE HOLDS OUTFILE
  local dir; dir=$(mktemp -d)
  mkfifo "$dir/ctl" "$dir/ack"
  EQ_PERF_CTL="$dir/ctl" EQ_PERF_ACK="$dir/ack" \
    perf stat --control "fifo:$dir/ctl,$dir/ack" --delay=-1 -x, \
      -e instructions,cycles,cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses,LLC-load-misses \
      -o "$3" -- "$BIN/bench_hold" --queue "$1" --dist exponential --n $N --holds "$2" --warmup $W > /dev/null 2> "$3.stderr"
  local st=$?
  rm -rf "$dir"
  return $st
}
perf_value() { awk -F, -v ev="$2" '$3 == ev || $3 ~ "^"ev"(:|$)" { gsub(/ /, "", $1); print $1; exit }' "$1"; }

run_cg() {                        # run_cg QUEUE HOLDS OUTFILE
  valgrind --tool=cachegrind --cache-sim=yes --instr-at-start=no --cachegrind-out-file=/dev/null \
    "$BIN/bench_hold" --queue "$1" --dist exponential --n $N --holds "$2" --warmup $W > /dev/null 2> "$3"
}
cg_value() { awk -v ev="$2" 'index($0, ev ":") { s = $0; sub(/^.*: */, "", s); split(s, a, " "); gsub(/,/, "", a[1]); print a[1]; exit }' "$1"; }

perf_ok() { command -v perf >/dev/null && perf stat -e instructions true >/dev/null 2>&1 && perf stat --help 2>&1 | grep -q -- --control; }
cg_ok() { command -v valgrind >/dev/null && "$BIN/bench_hold" --profile-support 2>/dev/null | grep -q '^cachegrind_regions=yes$'; }

tool=none
case ${EQ_PROFILER:-auto} in
  auto)       if perf_ok; then tool=perf; elif cg_ok; then tool=cachegrind; fi ;;
  perf)       perf_ok && tool=perf || note="EQ_PROFILER=perf but perf (with --control) is not usable here" ;;
  cachegrind) cg_ok && tool=cachegrind || note="EQ_PROFILER=cachegrind but valgrind or the binary's region support is missing" ;;
  none)       note="EQ_PROFILER=none" ;;
  *)          echo "profile.sh: EQ_PROFILER must be auto, perf, cachegrind or none" >&2; exit 2 ;;
esac
[[ $tool == cachegrind ]] && { N=100000; M=200000; W=200000; }   # ~50x slower: smaller, stated size

status=UNAVAILABLE
selfcheck="not run"
rows="queue,holds,instructions,instructions_per_hold,d_refs_or_l1_loads,l1d_misses,ll_misses,l1d_misses_per_hold,ll_misses_per_hold"
if [[ $tool != none ]]; then
  status=USABLE
  num() { [[ ${1:-} =~ ^[0-9]+$ ]] && echo "$1" || echo ""; }
  per() { [[ -n $1 ]] && awk -v a="$1" -v b="$2" 'BEGIN { printf "%.4f", a / b }' || echo "n/a"; }
  one=""
  for spec in "heap $M" "avl $M" "heap 1"; do
    set -- $spec; q=$1; m=$2; f="$OUT/${tool}_${q}_N${N}_M${m}.txt"
    if [[ $tool == perf ]]; then
      run_perf "$q" "$m" "$f"; st=$?
      ins=$(perf_value "$f" instructions); dr=$(perf_value "$f" L1-dcache-loads)
      d1=$(perf_value "$f" L1-dcache-load-misses); ll=$(perf_value "$f" LLC-load-misses)
    else
      run_cg "$q" "$m" "$f"; st=$?
      ins=$(cg_value "$f" "I refs"); dr=$(cg_value "$f" "D refs")
      d1=$(cg_value "$f" "D1  misses"); ll=$(cg_value "$f" "LLd misses")
    fi
    if (( st != 0 )); then status=FAILED; note="the $q run with M=$m exited with status $st (see $f*)"; break; fi
    ins=$(num "$ins")
    if [[ -z $ins || $ins == 0 ]]; then status=FAILED; note="no positive instruction count in $f"; break; fi
    d1=$(num "$d1"); ll=$(num "$ll"); dr=$(num "$dr")
    rows+=$'\n'"$q,$m,$ins,$(per "$ins" "$m"),${dr:-n/a},${d1:-n/a},${ll:-n/a},$(per "$d1" "$m"),$(per "$ll" "$m")"
    [[ $m == 1 ]] && one=$ins
  done
  if [[ $status == USABLE ]]; then
    if (( one > 0 && one < 100000 )); then
      selfcheck="PASSED (1 timed hold counted $one instructions, > 0 and < 100 000; init + warm-up are excluded)"
    else
      status=FAILED
      selfcheck="FAILED (1 timed hold counted $one instructions; expected > 0 and < 100 000)"
      note="region self-check failed: the counts are NOT restricted to the timed loop"
    fi
  fi
fi

if [[ $status == USABLE ]]; then echo "$rows" > "$OUT/summary.csv"
elif [[ $status == FAILED ]]; then echo "$rows" > "$OUT/summary_UNUSABLE.csv"; fi
echo "$status" > "$OUT/STATUS"
{
  echo "STATUS: $status"
  echo "profiler: $tool"
  [[ -n $note ]] && echo "note: $note"
  echo "workload: bench_hold --dist exponential --n $N --warmup $W; timed holds M=$M (heap, avl) and M=1 (heap, region self-check only)"
  echo "counted region: the timed hold loop only (bench::ProfileRegion); per-hold figures = count / holds"
  echo "region self-check: $selfcheck"
  case $status in
    USABLE)      echo "evidence: summary.csv and one raw output file per run"
                 [[ $tool == cachegrind ]] && echo "NOTE: cachegrind SIMULATES a cache (its own L1/LL model); it is not a hardware measurement. Quote it as simulated."
                 [[ $tool == perf ]] && echo "NOTE: hardware counters on the stated machine; event names differ by CPU — check the raw files" ;;
    UNAVAILABLE) echo "NO USABLE PROFILER on this machine (needs perf with --control, or valgrind with a binary built against cachegrind.h >= 3.22): REPORT.md must make no cache claims" ;;
    FAILED)      echo "PROFILING FAILED: summary_UNUSABLE.csv is kept only for diagnosis — REPORT.md must make no cache claims from this run" ;;
  esac
} > "$OUT/README.txt"
cat "$OUT/README.txt"
[[ $status == USABLE ]] && cat "$OUT/summary.csv"
case $status in USABLE) exit 0 ;; UNAVAILABLE) exit 5 ;; *) exit 1 ;; esac
