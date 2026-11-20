# Derivations D1–D6

`D<k>_<roll no>.pdf` is the submitted derivation, with its author's name and roll number on page 1 and a
"Where it is used" section naming the files and functions that rely on it; `D<k>.md` is its source
(`make pdfs` rebuilds the PDFs with pandoc + xelatex). D6's PDF is an export of `docs/MODEL_D6.md` (the
cost model, unchanged since it was committed before the measurements) followed by the "Where it is
used" section of `D6.md`. The drafts were prepared with Claude (declared in TOOLS.md); each author
checks every step of their own and can be asked to reproduce a step of anyone else's at the board in
the viva — so everyone should read all six. If you change a `.md`, run `make pdfs` and commit both.

| File | Author | Roll no. | Statement |
|---|---|---|---|
| `D1_250041016.pdf` | Gulam Abbas (Structure A owner) | 250041016 | lazy heap: m ≤ 2L, O(log n) amortised with purge after cancel and pop |
| `D2_250001009.pdf` | Aryan Arya (Structure B owner) | 250001009 | AVL of height h has ≥ F(h+3) − 1 nodes, so h < 1.45 log₂(n+2) |
| `D3_250003007.pdf` | Aditya Singh (Architect) | 250003007 | (time, seq) tie-breaking ⇒ deterministic run; timestamps never decrease |
| `D4_250041018.pdf` | Hiransh Anand (Oracle owner) | 250041018 | the AVL checker accepts exactly valid AVL trees |
| `D5_250041035.pdf` | Sanchit Goyal (Adversary) | 250041035 | ≤ 1 rebalance per insertion; ascending order rotates on all but O(log n) inserts |
| `D6_250001058.pdf` | Ram Upadhyay (Analyst) | 250001058 | the cost model and its predictions P1–P7 |
