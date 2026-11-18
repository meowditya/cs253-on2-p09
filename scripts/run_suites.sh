#!/usr/bin/env bash
# scripts/run_suites.sh — replay every adversarial / edge-case suite on all three queues and compare
# with the expected output; then check what the attacks do. Owner: Adversary (+ Oracle owner).
set -euo pipefail
export LC_ALL=C
BIN=${BIN:-build/release/bin}
S=adversary/suites
n=0
for f in "$S"/*.ops; do
  exp="${f%.ops}.expected"
  every=100; [[ $(basename "$f") == hand_* ]] && every=1
  for q in heap avl oracle; do
    if ! "$BIN/replay" --queue "$q" --check "$every" "$f" | cmp -s - "$exp"; then
      echo "SUITE FAILED: $f on $q"; exit 3
    fi
  done
  n=$((n + 1))
done
echo "suites: $n files x 3 queues match their expected output (hand-written ones checked after every op)"

stat() { "$BIN/replay" --queue "$1" --stats "$2" 2>&1 >/dev/null | tr ' ' '\n' | sed -n "s/^$3=//p"; }
# bloat: without purge the heap keeps every cancelled far timer; with purge it stays <= 2 x live
bnp=$(stat heap-nopurge "$S/gen_bloat.ops" physical); bp=$(stat heap "$S/gen_bloat.ops" physical); live=$(stat heap "$S/gen_bloat.ops" live)
[[ $bnp -ge 3000 && $bp -le $((2 * live)) ]] || { echo "bloat attack check failed ($bnp, $bp, $live)"; exit 3; }
# drain: the post-pop purge check matters
dnp=$(stat heap-nopurge "$S/gen_drain.ops" physical); dp=$(stat heap "$S/gen_drain.ops" physical)
[[ $dnp -eq 500 && $dp -eq 1 ]] || { echo "drain attack check failed ($dnp, $dp)"; exit 3; }
# ascending 1000: rebalance on all but floor(log2 1000)+1 = 10 inserts
r=$(stat avl "$S/gen_ascending.ops" rebalance_single)
[[ $r -eq 990 ]] || { echo "ascending attack check failed ($r)"; exit 3; }
echo "attacks: bloat ${bnp} entries without purge vs ${bp} with (live ${live}); drain ${dnp} vs ${dp}; ascending ${r}/1000 rebalances"
