#!/usr/bin/env bash
# scripts/make_pdfs.sh — export docs/derivations/D1..D5.md and docs/MODEL_D6.md (as D6) to PDF.
# Needs pandoc and xelatex with the DejaVu fonts (Linux: apt install pandoc texlive-xetex fonts-dejavu;
# macOS: brew install pandoc basictex). The PDFs are committed, so this is only needed after an edit.
set -euo pipefail
cd "$(dirname "$0")/.."
D=docs/derivations
for k in 1 2 3 4 5 6; do
  src=$D/D$k.md; [[ $k == 6 ]] && src=docs/MODEL_D6.md
  author=$(grep -m1 -o 'Author:\{0,1\}\**:\{0,1\} *[A-Z][a-z]* [A-Z][a-z]*' "$src" | sed 's/.*Author[:*]* *//')
  [[ -n $author ]] || { echo "make_pdfs: no author line in $src" >&2; exit 1; }
  pandoc -f markdown+lists_without_preceding_blankline "$src" -o "$D/D$k.pdf" --pdf-engine=xelatex -V geometry:margin=2.2cm -V fontsize=10pt \
    -V mainfont="DejaVu Sans" -V monofont="DejaVu Sans Mono" -V monofontoptions="Scale=0.82" \
    -V colorlinks=true -M author="$author" -M date="Team O(n²) · CS 253 / MA 253 · Project 9" 2> "$D/.pdf.log" \
    || { cat "$D/.pdf.log" >&2; exit 1; }
  if grep -q "Missing character" "$D/.pdf.log"; then echo "make_pdfs: missing glyphs in D$k:" >&2; grep "Missing character" "$D/.pdf.log" | sort -u | head >&2; rm -f "$D/.pdf.log"; exit 1; fi
  rm -f "$D/.pdf.log"
  echo "D$k.pdf  author: $author"
done
