#!/usr/bin/env python3
"""tests/plot_test.py — results/plot.py must reject every inconsistent dataset BEFORE writing anything,
and must classify exact ties as ties. Owner: Analyst.

Builds a small SYNTHETIC but internally consistent dataset for plot.py's "selftest" matrix in a
temporary directory (the numbers are invented test fixtures, never results), checks that it is
accepted, then applies one defect at a time and checks that plot.py exits 1 and leaves no outputs.
"""
import csv
import importlib.util
import os
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("plot", os.path.join(HERE, "..", "results", "plot.py"))
plot = importlib.util.module_from_spec(spec)
spec.loader.exec_module(plot)

FAILS = []
SPEED = {"heap": 100, "avl": 150, "oracle": 300, "control": 20}     # fixture ns per operation


def fixture():
    data = {}
    for name, (cols, build, kind) in plot.FILES.items():
        rows = []
        for (q, k), _ in sorted(plot.expected("selftest")[name].items(), key=lambda kv: tuple(map(str, kv[0]))):
            r = {c: "0" for c in cols}
            r.update(experiment=kind, version=plot.VERSION, build=build, queue=q, order="0")
            if kind == "hold":
                for c, v in zip(("dist", "n", "warmup", "holds", "seed", "repeat", "mean_us"), k):
                    r[c] = str(v)
                m, unit = int(r["holds"]), "hold"
                r["zero_inc_frac"] = "0.000000"
            else:
                for c, v in zip(("policy", "dist", "n", "warmup", "steps", "cancel_bp", "seed", "repeat", "mean_us"), k):
                    r[c] = str(v)
                m, unit = int(r["steps"]), "step"
                r["cancels"] = str(plot.cancels_for(int(r["cancel_bp"]), m))
            set_time(r, m * SPEED[q], m, unit)
            r["checksum"] = str(hash(k) & 0x7FFFFFFFFFFFFFFF)
            rows.append(r)
        data[name] = (cols, rows)
    return data


def set_time(r, elapsed, m, unit):
    r["elapsed_ns"] = str(elapsed)
    ns = elapsed / m
    r[f"ns_per_{unit}"] = f"{ns:.3f}"
    r[f"{unit}s_per_sec"] = f"{1e9 / ns:.1f}"


def write(root, data):
    raw = os.path.join(root, "results", "raw")
    os.makedirs(raw, exist_ok=True)
    for name, (cols, rows) in data.items():
        with open(os.path.join(raw, name), "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=cols)
            w.writeheader()
            w.writerows(rows)


def outputs(root):
    figs = os.path.join(root, "results", "figures")
    return [p for p in (os.path.join(root, "results", "summary.csv"), os.path.join(root, "results", "paired_ratios.csv"))
            if os.path.exists(p)] + ([f for f in os.listdir(figs) if f.endswith(".png")] if os.path.isdir(figs) else [])


def run(root):
    devnull = open(os.devnull, "w")
    old = sys.stdout, sys.stderr
    sys.stdout = sys.stderr = devnull
    try:
        return plot.main(["--mode", "selftest", "--root", root])
    finally:
        sys.stdout, sys.stderr = old
        devnull.close()


def find(rows, **kw):
    return next(r for r in rows if all(r[k] == str(v) for k, v in kw.items()))


def expect_reject(what, mutate):
    root = tempfile.mkdtemp()
    try:
        data = fixture()
        write(root, data)
        if run(root) != 0 or not outputs(root):                    # leave valid outputs behind first
            FAILS.append(f"{what}: the valid fixture was not accepted")
            return
        mutate(data)
        write(root, data)
        st = run(root)
        left = outputs(root)
        if st != 1 or left:
            FAILS.append(f"{what}: exit {st}, outputs left {left[:3]}")
    finally:
        shutil.rmtree(root)


def hold_rows(d):
    return d["hold.csv"][1]


def sweep_rows(d):
    return d["sweep.csv"][1]


def m_warmup(d):
    r = find(hold_rows(d), queue="avl", dist="uniform", n=1000, seed=1)
    r["warmup"], r["checksum"] = "17", "123456789"                  # the review's reproduction


def m_holds(d):
    r = find(hold_rows(d), queue="avl", dist="uniform", n=1000, seed=1)
    r["holds"] = "999"
    set_time(r, 999 * 150, 999, "hold")


def m_steps(d):
    r = find(sweep_rows(d), queue="avl", policy="uniform", cancel_bp=5000, seed=1)
    r["steps"] = "999"
    r["cancels"] = str(plot.cancels_for(5000, 999))
    set_time(r, 999 * 150, 999, "step")


def m_checksum(d):
    find(hold_rows(d), queue="avl", dist="bimodal", n=1000, seed=2)["checksum"] = "42"


def m_duplicate(d):
    hold_rows(d).append(dict(find(hold_rows(d), queue="avl", dist="uniform", n=1000, seed=1)))


def m_missing_mate(d):
    rows = hold_rows(d)
    rows.remove(find(rows, queue="heap", dist="triangular", n=1000, seed=2))


def m_incomplete(d):
    rows = sweep_rows(d)
    for q in ("heap", "avl"):
        rows.remove(find(rows, queue=q, policy="latest", cancel_bp=9500, seed=2))


def m_extra(d):
    r = dict(find(hold_rows(d), queue="heap", dist="uniform", n=1000, seed=1))
    r["seed"] = "3"
    hold_rows(d).append(r)


def m_cancels(d):
    find(sweep_rows(d), queue="heap", policy="uniform", cancel_bp=5000, seed=1)["cancels"] = "400"


def m_cancels_over(d):
    r = find(sweep_rows(d), queue="heap", policy="uniform", cancel_bp=9500, seed=1)
    r["cancels"] = str(int(r["steps"]) + 1)


def m_zero_frac(d):
    find(hold_rows(d), queue="heap", dist="uniform", n=1000, seed=1)["zero_inc_frac"] = "1.5"


def m_tput_text(d):
    find(hold_rows(d), queue="heap", dist="uniform", n=1000, seed=1)["holds_per_sec"] = "123.0"


def m_nan(d):
    find(sweep_rows(d), queue="heap", policy="uniform", cancel_bp=0, seed=1)["steps_per_sec"] = "-nan"


def m_build(d):
    find(hold_rows(d), queue="heap", dist="uniform", n=1000, seed=1)["build"] = "count"


def m_header_only(d):
    d["sweep_count.csv"][1].clear()


for what, fn in [("mismatched warm-up (review R1)", m_warmup), ("mismatched holds", m_holds),
                 ("mismatched steps", m_steps), ("changed checksum on a matching pair", m_checksum),
                 ("exact duplicate row", m_duplicate), ("missing queue mate", m_missing_mate),
                 ("incomplete matrix", m_incomplete), ("row outside the matrix", m_extra),
                 ("cancels inconsistent with cancel_bp", m_cancels), ("cancels > steps", m_cancels_over),
                 ("zero_inc_frac > 1", m_zero_frac), ("throughput text inconsistent", m_tput_text),
                 ("nan throughput", m_nan), ("count row in a timing file", m_build),
                 ("header-only file", m_header_only)]:
    expect_reject(what, fn)

# exact ties are ties, not an AVL win (review R5)
root = tempfile.mkdtemp()
try:
    data = fixture()
    for s in (1, 2):
        a = find(hold_rows(data), queue="avl", dist="uniform", n=1000, seed=s)
        set_time(a, int(a["holds"]) * SPEED["heap"], int(a["holds"]), "hold")
    write(root, data)
    st = run(root)
    with open(os.path.join(root, "results", "summary.csv")) as f:
        line = next((l for l in f if l.startswith("selftest,ratio_hold,heap/avl,uniform,1000,")), "")
    if st != 0 or "ties=2;all_same_side=tie" not in line:
        FAILS.append(f"tie classification: exit {st}, row {line.strip()}")
    with open(os.path.join(root, "results", "summary.csv")) as f:
        other = next((l for l in f if l.startswith("selftest,ratio_hold,heap/avl,bimodal,1000,")), "")
    if "heap_faster=2;avl_faster=0;ties=0;all_same_side=heap" not in other:
        FAILS.append(f"heap side classification: row {other.strip()}")
finally:
    shutil.rmtree(root)

if FAILS:
    for f in FAILS:
        print("FAIL: " + f)
    print(f"plot_test: {len(FAILS)} FAILED")
    sys.exit(1)
print("plot_test: all checks passed")
