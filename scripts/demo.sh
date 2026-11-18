#!/usr/bin/env bash
# scripts/demo.sh — the live demonstration (run from a fresh clone of the `final` tag). Owner: Architect.
set -euo pipefail
export LC_ALL=C
BIN=${BIN:-build/release/bin}
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
echo "== 1. Simulator, 10^6 dispatched events, default parameters, on the heap:"
"$BIN/sim" --queue heap | tee "$tmp/heap.out"
echo; echo "== 2. Same run on the AVL tree (timing on stderr):"
"$BIN/sim" --queue avl > "$tmp/avl.out"
cmp "$tmp/heap.out" "$tmp/avl.out" && echo "   -> byte-identical output on heap and AVL"
echo; echo "== 3. Seeds 2 and 3, and a traced run checked against the oracle:"
BIN="$BIN" scripts/check_identical.sh
echo; echo "== 4. Full-scale occupancy: 10^6 events resident at once"
"$BIN/gen" ascending 1000000 > "$tmp/asc.ops"
for q in heap avl; do "$BIN/replay" --queue "$q" --stats "$tmp/asc.ops" > /dev/null; done
echo; echo "== 5. The heap-bloat attack with and without purging:"
"$BIN/gen" bloat 1000 200000 > "$tmp/bloat.ops"
for q in heap-nopurge heap; do "$BIN/replay" --queue "$q" --stats "$tmp/bloat.ops" > /dev/null; done
