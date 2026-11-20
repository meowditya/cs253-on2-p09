#!/usr/bin/env bash
# scripts/run_all.sh — the one command of the submission guidelines: build, test, run the application,
# run every benchmark, regenerate every figure. Owner: Architect.
#
#   scripts/run_all.sh            everything (about 13 min on an Apple M4; the benchmarks dominate)
#   scripts/run_all.sh --figures  only redraw results/figures/*.png (and the summaries and REPORT.md
#                                 §10.1/§11) from the committed CSVs in results/raw — a few seconds
#   scripts/run_all.sh --quick    the same steps as the default with a ~1-minute smoke benchmark; it
#                                 replaces results/raw and the figures with QUICK data (not for the
#                                 report: `git checkout results REPORT.md` restores the real ones)
#
# The default is a thin wrapper around the Makefile targets, run strictly one after another so that
# the tests, the demo and the timed benchmarks never overlap:
#   make build → make test → make demo → make bench   (= `make all`)
# `make bench` writes results/raw/*.csv, then results/plot.py validates them and writes
# results/figures/*.png, results/summary.csv and results/paired_ratios.csv, and
# scripts/fill_report.py copies the numbers into REPORT.md §10.1 and §11.
set -euo pipefail
cd "$(dirname "$0")/.."

bench=bench
case "${1:-}" in
  "") ;;
  --quick) bench=bench-quick ;;
  --figures)
    echo "==== results/plot.py: validate results/raw/*.csv, redraw results/figures/*.png"
    python3 results/plot.py --mode full
    echo "==== scripts/fill_report.py: REPORT.md §10.1 and §11 from results/"
    python3 scripts/fill_report.py --mode full
    exit 0 ;;
  -h|--help) sed -n '2,17p' "$0"; exit 0 ;;
  *) echo "usage: scripts/run_all.sh [--figures | --quick]" >&2; exit 2 ;;
esac

for target in build test demo "$bench"; do
  echo "==== make $target"
  make "$target"
done
echo "==== run_all: build, tests, demo and $bench finished"
