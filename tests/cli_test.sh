#!/usr/bin/env bash
# tests/cli_test.sh — command-line hardening regressions (review findings F6, F7, F8, F11, R4).
# Needs only bash and python3 (time limits via tests/run_timeout.py, not GNU timeout).
# Every bad invocation must fail FAST with the documented exit status, never hang or print
# a result row. Owner: Oracle owner (gen), Analyst (benchmarks, plot.py).
set -uo pipefail
export LC_ALL=C
BIN=${BIN:-build/release/bin}
fails=0
expect() {                       # expect STATUS description -- command...
  local want=$1 what=$2; shift 3
  local out; out=$(python3 tests/run_timeout.py 10 "$@" 2>&1 >/dev/null); local got=$?
  if [[ $got != "$want" ]]; then echo "FAIL: $what: exit $got, expected $want ($out)"; fails=$((fails + 1)); fi
}
# gen: argument ranges and exact argument counts (F6: `gen bloat 0 1` used to loop forever)
expect 2 "gen bloat L=0"            -- "$BIN/gen" bloat 0 1
expect 2 "gen latesucc N=0"         -- "$BIN/gen" latesucc 0 5
expect 2 "gen fib-delete H=31"      -- "$BIN/gen" fib-delete 31 1
expect 2 "gen extra argument"       -- "$BIN/gen" ascending 5 9
expect 2 "gen missing argument"     -- "$BIN/gen" random 1 100
expect 2 "gen not a number"         -- "$BIN/gen" ascending 5x
expect 2 "gen unknown mode"         -- "$BIN/gen" nosuchmode 5
expect 0 "gen valid"                -- "$BIN/gen" bloat 3 4
# benchmarks: zero/oversized counts are rejected instead of printing inf / -nan (F7, F11)
expect 2 "bench_hold --holds 0"     -- "$BIN/bench_hold" --queue heap --n 1 --warmup 0 --holds 0
expect 2 "bench_hold --n 0"         -- "$BIN/bench_hold" --queue heap --n 0 --holds 10
expect 2 "bench_hold --mean-us 0"   -- "$BIN/bench_hold" --queue heap --n 10 --holds 10 --mean-us 0
expect 2 "bench_hold unknown queue" -- "$BIN/bench_hold" --queue nope --n 10 --holds 10
expect 2 "bench_hold nopurge"       -- "$BIN/bench_hold" --queue heap-nopurge --n 10 --holds 10
expect 2 "bench_sweep --steps 0"    -- "$BIN/bench_sweep" --queue avl --n 10 --steps 0
expect 2 "bench_sweep huge steps"   -- "$BIN/bench_sweep" --queue avl --n 10 --steps 2000000000
expect 2 "bench_sweep bp > 10000"   -- "$BIN/bench_sweep" --queue avl --n 10 --steps 10 --cancel-bp 10001
expect 2 "bench_sweep bad policy"   -- "$BIN/bench_sweep" --queue avl --n 10 --steps 10 --policy oldest
expect 0 "bench_hold valid"         -- "$BIN/bench_hold" --queue avl --n 10 --holds 100 --warmup 10
expect 0 "bench_sweep valid"        -- "$BIN/bench_sweep" --queue heap --n 10 --steps 100 --cancel-bp 9500 --policy latest
# write failures are errors, never a silent success (R4): exit 4 and no result
if [[ -w /dev/full ]]; then
  expect 4 "bench_hold --csv /dev/full"  -- "$BIN/bench_hold" --queue heap --n 1 --holds 10 --warmup 0 --csv /dev/full
  expect 4 "bench_sweep --csv /dev/full" -- "$BIN/bench_sweep" --queue heap --n 10 --steps 10 --csv /dev/full
  python3 tests/run_timeout.py 10 "$BIN/bench_hold" --queue heap --n 1 --holds 10 --warmup 0 > /dev/full 2>/dev/null
  st=$?; [[ $st == 4 ]] || { echo "FAIL: bench_hold stdout to /dev/full: exit $st, expected 4"; fails=$((fails + 1)); }
else
  echo "note: no /dev/full here (macOS), write-failure checks skipped"
fi
expect 4 "bench_hold unwritable csv dir" -- "$BIN/bench_hold" --queue heap --n 1 --holds 10 --warmup 0 --csv /nonexistent-dir/x.csv
expect 0 "bench_hold --profile-support" -- "$BIN/bench_hold" --profile-support
# replay / difftest
expect 2 "replay missing file"      -- "$BIN/replay" --queue heap /nonexistent.ops
expect 2 "difftest bad option"      -- "$BIN/difftest" --nosuch 1
# plot.py: a header-only CSV must be an error, not "0 rows" with exit 0 (F8)
if python3 -c "import matplotlib" 2>/dev/null; then
  d=$(mktemp -d); mkdir -p "$d/results/raw"
  "$BIN/bench_hold" --queue heap --n 10 --holds 10 --warmup 0 --csv "$d/h.csv" >/dev/null
  head -1 "$d/h.csv" > "$d/results/raw/hold.csv"
  "$BIN/bench_sweep" --queue heap --n 10 --steps 10 --csv "$d/s.csv" >/dev/null
  head -1 "$d/s.csv" > "$d/results/raw/sweep.csv"
  script=$(pwd)/results/plot.py
  out=$(cd "$d" && python3 "$script" --mode quick 2>&1); st=$?
  if [[ $st == 0 ]]; then echo "FAIL: plot.py accepted header-only CSVs ($out)"; fails=$((fails + 1)); fi
  rm -rf "$d"
else
  echo "note: matplotlib missing, plot.py check skipped"
fi
if [[ $fails == 0 ]]; then echo "cli_test: all checks passed"; else echo "cli_test: $fails FAILED"; exit 1; fi
