#!/usr/bin/env bash
# scripts/check_identical.sh — the application must print byte-identical stdout on every correct
# queue. Owner: Architect.   Usage: BIN=build/release/bin scripts/check_identical.sh [--small-only]
set -euo pipefail
export LC_ALL=C
BIN=${BIN:-build/release/bin}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
keep=failures/identical; fail() { mkdir -p "$keep"; cp "$tmp"/*.out "$keep"/ 2>/dev/null || true; echo "DIFFERENT OUTPUT: $1 (outputs kept in $keep/)"; exit 3; }

for seed in 1 2 3; do                       # every event and monitor query traced, all three queues
  for q in heap avl oracle; do "$BIN/sim" --queue "$q" --seed "$seed" --flows 2000 --events 100000 --trace > "$tmp/small_$q.out" 2>/dev/null; done
  cmp -s "$tmp/small_heap.out" "$tmp/small_avl.out"    || fail "traced seed $seed: heap vs avl"
  cmp -s "$tmp/small_heap.out" "$tmp/small_oracle.out" || fail "traced seed $seed: heap vs oracle"
done
echo "identical traced output (1e5 events, seeds 1-3): heap = avl = oracle"
[[ "${1:-}" == "--small-only" ]] && exit 0
for seed in 1 2 3; do                       # full scale: 10^6 dispatched events, heap vs avl
  for q in heap avl; do "$BIN/sim" --queue "$q" --seed "$seed" > "$tmp/full_$q.out" 2>/dev/null; done
  cmp -s "$tmp/full_heap.out" "$tmp/full_avl.out" || fail "full scale seed $seed: heap vs avl"
done
echo "identical summaries (1e6 events, seeds 1-3): heap = avl"
