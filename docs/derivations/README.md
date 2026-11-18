# Derivations D1–D6

`Dk.pdf` is the submitted derivation, with its author on page 1; `Dk.md` is its source
(`make pdfs` rebuilds the PDFs with pandoc + xelatex). The drafts were prepared with Claude (declared in
TOOLS.md); each author checks every step of their own and can be asked to reproduce a step of anyone
else's at the board in the viva — so everyone should read all six. If you change a `.md`, run
`make pdfs` and commit both.

| File | Author | Statement |
|---|---|---|
| D1 | Gulam Abbas (Structure A owner) | lazy heap: m ≤ 2L, O(log n) amortised with purge after cancel and pop |
| D2 | Aryan Arya (Structure B owner) | AVL of height h has ≥ F(h+3) − 1 nodes, so h < 1.45 log₂(n+2) |
| D3 | Aditya Singh (Architect) | (time, seq) tie-breaking ⇒ deterministic run; timestamps never decrease |
| D4 | Hiransh Anand (Oracle owner) | the AVL checker accepts exactly valid AVL trees |
| D5 | Sanchit Goyal (Adversary) | ≤ 1 rebalance per insertion; ascending order rotates on all but O(log n) inserts |
| D6 | Ram Upadhyay (Analyst) | the cost model: D6.pdf is a full export of `docs/MODEL_D6.md` |
