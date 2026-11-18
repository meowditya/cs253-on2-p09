#!/usr/bin/env bash
# scripts/run_tests.sh — every correctness test, in order. Called by `make test` / `make sanitize`.
set -euo pipefail
export LC_ALL=C
BIN=${BIN:-build/release/bin}
SAN=0; [[ "${1:-}" == "--sanitize" ]] && SAN=1
# Leak detection: on by default on Linux. Apple's toolchain has no LeakSanitizer, so it is off on macOS
# (run `make sanitize` on Linux/WSL for the leak check). LSan also cannot run under ptrace (debuggers,
# some sandboxes): there, set ASAN_OPTIONS=detect_leaks=0 yourself and say so wherever you report it.
if [[ -z ${ASAN_OPTIONS:-} ]]; then
  if [[ $(uname -s) == Darwin ]]; then export ASAN_OPTIONS=detect_leaks=0; else export ASAN_OPTIONS=detect_leaks=1; fi
fi
step() { echo; echo "== $*"; }
[[ $SAN == 1 ]] && echo "sanitizer run: ASAN_OPTIONS=$ASAN_OPTIONS (leak detection $([[ $ASAN_OPTIONS == *detect_leaks=1* ]] && echo ON || echo OFF))"

step "unit tests"
for t in interface_contract_test test_opstream test_structA test_structB test_oracle test_checkers_mutations test_sim; do
  "$BIN/$t"
done

step "command-line hardening, profiler logic, CSV validation"
BIN="$BIN" tests/cli_test.sh
BIN="$BIN" tests/profile_test.sh
if python3 -c "import matplotlib" 2>/dev/null; then python3 tests/plot_test.py; else echo "note: matplotlib missing, plot_test skipped"; fi

step "adversarial and edge-case suites"
BIN="$BIN" scripts/run_suites.sh

step "differential test: oracle = heap = avl"
ops=1000000; [[ $SAN == 1 ]] && ops=200000
for seed in 1 2 3; do "$BIN/difftest" --seed "$seed" --ops "$ops"; done

step "full scale: 10^6 resident events, 10^6 operations"
"$BIN/test_fullscale"

step "application: identical output on every queue"
if [[ $SAN == 1 ]]; then BIN="$BIN" scripts/check_identical.sh --small-only; else BIN="$BIN" scripts/check_identical.sh; fi

echo; echo "ALL TESTS PASSED"
