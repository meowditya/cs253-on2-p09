#!/usr/bin/env bash
# scripts/make_pdfs.sh — export the derivations to docs/derivations/D<k>_<roll no>.pdf (the names the
# submission guidelines require), and REPORT.md to REPORT.pdf.
#   D1..D5: docs/derivations/D<k>.md
#   D6:     docs/MODEL_D6.md (the cost model, left unchanged since its commit before the measurements)
#           followed by the "Where it is used" part of docs/derivations/D6.md
# Author and roll number are read from the "**Author:** … **Roll no.** …" line of D<k>.md.
# Needs pandoc and xelatex with the DejaVu fonts (Linux: apt install pandoc texlive-xetex fonts-dejavu;
# macOS: brew install pandoc basictex). The PDFs are committed, so this is only needed after an edit.
set -euo pipefail
cd "$(dirname "$0")/.."
D=docs/derivations
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

# Pipe tables get column widths in proportion to the dashes of their separator row; size each column by
# its longest cell (square-root damped) so that wide tables wrap sensibly, and set tables in small type.
widths() {
  python3 - "$1" "$2" <<'PY'
import math, re, sys
lines = open(sys.argv[1]).read().split("\n")
out, i = [], 0
while i < len(lines):
    if (i + 1 < len(lines) and lines[i].startswith("|") and re.fullmatch(r"\|(\s*:?-+:?\s*\|)+", lines[i + 1].strip())):
        j = i + 2
        while j < len(lines) and lines[j].startswith("|"):
            j += 1
        rows = [[c.strip() for c in l.strip().strip("|").split(" | ")] for l in [lines[i]] + lines[i + 2:j]]
        n = len(rows[0])
        w = [max(len(r[k]) if k < len(r) else 0 for r in rows) for k in range(n)]
        d = [max(3, round(4 * math.sqrt(x))) for x in w]
        out += [lines[i], "|" + "|".join("-" * x for x in d) + "|"] + lines[i + 2:j]
        i = j
    else:
        out.append(lines[i]); i += 1
open(sys.argv[2], "w").write("\n".join(out))
PY
}
printf '%s\n' '\usepackage{etoolbox}' '\AtBeginEnvironment{longtable}{\footnotesize}' > "$tmp/head.tex"

pdf() {  # pdf <markdown> <output.pdf> <author line> [resource path]
  widths "$1" "$tmp/w.md"
  pandoc -f markdown+lists_without_preceding_blankline "$tmp/w.md" -o "$2" --pdf-engine=xelatex -H "$tmp/head.tex" \
    --columns=100 --resource-path="${4:-.}" -V geometry:margin=2cm -V fontsize=10pt \
    -V mainfont="DejaVu Sans" -V monofont="DejaVu Sans Mono" -V monofontoptions="Scale=0.82" \
    -V colorlinks=true -M author="$3" -M date="Team O(n²) · CS 253 / MA 253 · Project 9" 2> "$tmp/log" \
    || { cat "$tmp/log" >&2; exit 1; }
  if grep -q "Missing character" "$tmp/log"; then
    echo "make_pdfs: missing glyphs in $2:" >&2; grep "Missing character" "$tmp/log" | sort -u | head >&2; exit 1
  fi
}

for k in 1 2 3 4 5 6; do
  meta=$D/D$k.md
  author=$(grep -m1 -o 'Author:\** *[A-Z][a-z]* [A-Z][a-z]*' "$meta" | sed 's/.*Author[:*]* *//')
  roll=$(grep -m1 -o 'Roll no\.\** *[0-9]\{9\}' "$meta" | grep -o '[0-9]\{9\}')
  [[ -n $author && -n $roll ]] || { echo "make_pdfs: no author or roll number in $meta" >&2; exit 1; }
  src=$meta
  if [[ $k == 6 ]]; then        # the model, then this file's "Where it is used" section onwards
    src=$tmp/D6.md
    # name and roll number under the model's title (page 1); docs/MODEL_D6.md itself stays unchanged
    { awk -v a="$author" -v r="$roll" '{ print } !done && /^# / { print ""; print "**" a " · Roll no. " r "**"; done = 1 }' docs/MODEL_D6.md
      echo; sed -n '/^## Where it is used/,$p' "$meta"; } > "$src"
  fi
  rm -f "$D/D$k.pdf" "$D"/D${k}_*.pdf
  pdf "$src" "$D/D${k}_$roll.pdf" "$author · Roll no. $roll"
  echo "D${k}_$roll.pdf  author: $author"
done

pdf REPORT.md REPORT.pdf "Team O(n²): Aditya Singh, Gulam Abbas, Aryan Arya, Hiransh Anand, Sanchit Goyal, Ram Upadhyay" .
echo "REPORT.pdf"
