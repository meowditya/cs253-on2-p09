#!/usr/bin/env bash
# tests/profile_test.sh — scripts/profile.sh must report USABLE / UNAVAILABLE / FAILED correctly, and the
# benchmark must honour perf's enable/disable/ack protocol. Uses a FAKE perf (tests/fakes/perf): this
# tests the script's logic only, never hardware counts. Owner: Analyst.
set -uo pipefail
export LC_ALL=C
BIN=${BIN:-build/release/bin}
ROOT=$(pwd)
TO="python3 $ROOT/tests/run_timeout.py"
fails=0
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/bin"
cp tests/fakes/perf "$tmp/bin/perf"
prof() {                    # prof WANT_EXIT WANT_STATUS DESCRIPTION [VAR=VALUE...]
  local want=$1 wst=$2 what=$3; shift 3
  rm -rf "$tmp/out"
  env PATH="$tmp/bin:$PATH" PROFILE_OUT="$tmp/out" BIN="$BIN" "$@" $TO 60 scripts/profile.sh --quick > "$tmp/log" 2>&1
  local got=$? st; st=$(cat "$tmp/out/STATUS" 2>/dev/null)
  if [[ $got != "$want" || $st != "$wst" ]]; then
    echo "FAIL: $what: exit $got / STATUS '$st', expected $want / $wst"; fails=$((fails + 1)); return
  fi
  if [[ $wst == USABLE && ! -s $tmp/out/summary.csv ]]; then echo "FAIL: $what: no summary.csv"; fails=$((fails + 1)); fi
  if [[ $wst != USABLE && -e $tmp/out/summary.csv ]]; then echo "FAIL: $what: summary.csv written although $wst"; fails=$((fails + 1)); fi
}
prof 0 USABLE      "plausible counts"                   EQ_PROFILER=perf
prof 1 FAILED      "self-check: 1 hold counts 200000"    EQ_PROFILER=perf FAKE_ONE=200000
prof 1 FAILED      "self-check: 1 hold counts 0"         EQ_PROFILER=perf FAKE_ONE=0
prof 1 FAILED      "main run counts 0 instructions"      EQ_PROFILER=perf FAKE_MAIN=0
prof 1 FAILED      "main run counts garbage"             EQ_PROFILER=perf FAKE_MAIN=abc
prof 1 FAILED      "benchmark exits nonzero under perf"  EQ_PROFILER=perf FAKE_EXIT=3
prof 5 UNAVAILABLE "profiling switched off"              EQ_PROFILER=none
$TO 10 scripts/profile.sh --bogus > /dev/null 2>&1; [[ $? == 2 ]] || { echo "FAIL: bad mode not exit 2"; fails=$((fails + 1)); }

# The benchmark's side of perf's control protocol, driven by a scripted responder.
responder() {               # responder REPLY: answer two commands (enable, disable) with REPLY
  for _ in 1 2; do read -r _ < "$tmp/ctl"; echo "$1" > "$tmp/ack"; done
}
mkfifo "$tmp/ctl" "$tmp/ack"
for reply in ack nak; do
  responder "$reply" &
  rp=$!
  EQ_PERF_CTL="$tmp/ctl" EQ_PERF_ACK="$tmp/ack" $TO 20 "$BIN/bench_hold" --queue heap --n 10 --holds 10 --warmup 0 > /dev/null 2>&1
  got=$?
  kill "$rp" 2>/dev/null; wait "$rp" 2>/dev/null
  want=0; [[ $reply == nak ]] && want=3
  [[ $got == "$want" ]] || { echo "FAIL: perf control with reply '$reply': exit $got, expected $want"; fails=$((fails + 1)); }
done
EQ_PERF_CTL="$tmp/missing" EQ_PERF_ACK="$tmp/missing2" $TO 20 "$BIN/bench_hold" --queue heap --n 10 --holds 10 --warmup 0 > /dev/null 2>&1
[[ $? == 3 ]] || { echo "FAIL: unreachable perf control FIFO not exit 3"; fails=$((fails + 1)); }

if [[ $fails == 0 ]]; then echo "profile_test: all checks passed"; else echo "profile_test: $fails FAILED"; exit 1; fi
