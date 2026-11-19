#!/usr/bin/env python3
"""results/plot.py — validate results/raw/*.csv, then write results/summary.csv,
results/paired_ratios.csv and results/figures/*.png. Owner: Analyst.
Needs only the Python standard library and matplotlib.

    python3 results/plot.py [--mode full|quick] [--root DIR]     (DIR defaults to the current directory)

ONE canonical workload key per benchmark is used for every check, for pairing and for aggregation:
    hold : (dist, n, warmup, holds, seed, repeat, mean_us)
    sweep: (policy, dist, n, warmup, steps, cancel_bp, seed, repeat, mean_us)
Validation runs to completion BEFORE anything is written; on any error the script exits 1, writes
nothing, and removes stale outputs of an earlier run (summary.csv, paired_ratios.csv, figures/*.png):
  * every file present, CSV version 3 header, at least one row, the right build (release / count);
  * per row: integers where integers belong; timings and throughputs finite and > 0; n, holds, steps
    >= 1; 0 <= zero_inc_frac <= 1; cancel_bp <= 10000; cancels == round(cancel_bp/10000 * steps)
    exactly as bench_sweep computes it; ns/op and ops/s consistent with elapsed_ns;
  * no duplicate (workload key, queue);
  * the rows are EXACTLY the matrix of the chosen mode (scripts/run_bench.sh) with its prescribed
    settings — N, distribution, seed, warm-up, timed length, mean, repeat — nothing missing, nothing
    extra; so heap and AVL exist for every workload and five seeds per point in full mode;
  * every queue on the same workload has the same popped-key checksum.
Throughput is recomputed from the exact integers (ops * 1e9 / elapsed_ns), not from rounded text.
Heap vs AVL: paired ratio heap/AVL throughput on the same workload key; every ratio is written to
paired_ratios.csv. A point is called for one side only if EVERY ratio is strictly on that side; exact
ties are counted separately ("tie"), anything else is "mixed". That is an empirical consistency
criterion, not a significance test.
Quick mode is a smoke test only: its figures and summary are labelled "QUICK — not for the report".
Mode "selftest" is a tiny synthetic matrix used only by tests/plot_test.py.
"""
import argparse
import csv
import glob
import math
import os
import statistics as st
import sys
from collections import defaultdict

VERSION = "3"
HOLD_COLS = ("experiment version build queue dist n warmup holds seed repeat order mean_us elapsed_ns ns_per_hold "
             "holds_per_sec checksum zero_inc_frac peak_physical purges rebalance_single rebalance_double comparisons "
             "moves hash_probes hash_rehash_probes table_rebuilds table_rebuild_slots bytes").split()
SWEEP_COLS = ("experiment version build queue policy dist n warmup steps cancels cancel_bp seed repeat order mean_us "
              "elapsed_ns ns_per_step steps_per_sec checksum peak_physical purges purged stale_discards rebalance_single "
              "rebalance_double comparisons moves hash_probes hash_rehash_probes table_rebuilds table_rebuild_slots "
              "bytes").split()
INT_COLS = {"n", "warmup", "holds", "steps", "cancels", "cancel_bp", "seed", "repeat", "mean_us", "elapsed_ns",
            "checksum", "peak_physical", "purges", "purged", "stale_discards", "rebalance_single", "rebalance_double",
            "comparisons", "moves", "hash_probes", "hash_rehash_probes", "table_rebuilds", "table_rebuild_slots", "bytes"}
FLOAT_COLS = {"ns_per_hold", "holds_per_sec", "ns_per_step", "steps_per_sec", "zero_inc_frac"}
DISTS = ["exponential", "uniform", "bimodal", "triangular"]
HA = ("heap", "avl")
FILES = {  # file: (columns, build, kind)
    "hold.csv": (HOLD_COLS, "release", "hold"),
    "sweep.csv": (SWEEP_COLS, "release", "sweep"),
    "hold_count.csv": (HOLD_COLS, "count", "hold"),
    "sweep_count.csv": (SWEEP_COLS, "count", "sweep"),
}

# The experiment matrix of scripts/run_bench.sh, per mode. Keep the two files in step.
# holds/warmup None = bench_hold's defaults: holds max(10^6, N), warm-up max(10^6, 5N).
MATRIX = {
    "full": dict(ns=[1000, 10000, 100000, 1000000], seeds=[1, 2, 3, 4, 5], holds=None, warmup=None,
                 small_ns=[10, 30, 100, 300], sens_n=100000, sweep_n=1000000, sweep_steps=1000000,
                 cs=[0, 1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 9500], scale_ns=[10000, 100000]),
    "quick": dict(ns=[1000, 10000, 100000], seeds=[1, 2], holds=200000, warmup=200000, small_ns=[10, 100],
                  sens_n=10000, sweep_n=100000, sweep_steps=200000, cs=[0, 5000, 9500], scale_ns=[]),
    "selftest": dict(ns=[1000], seeds=[1, 2], holds=1000, warmup=1000, small_ns=[10], sens_n=1000,
                     sweep_n=1000, sweep_steps=1000, cs=[0, 5000, 9500], scale_ns=[100]),
}
SMALL_HOLDS, SMALL_WARMUP = 1000000, 100000          # run_bench.sh's small-N runs set these explicitly


def hold_key(r):
    return (r["dist"], r["n"], r["warmup"], r["holds"], r["seed"], r["repeat"], r["mean_us"])


def sweep_key(r):
    return (r["policy"], r["dist"], r["n"], r["warmup"], r["steps"], r["cancel_bp"], r["seed"], r["repeat"], r["mean_us"])


def expected(mode):
    """{file: {(queue, canonical key): experiment}} — exactly what scripts/run_bench.sh produces."""
    M = MATRIX[mode]

    def hw(n):
        return ((M["holds"], M["warmup"]) if M["holds"] is not None
                else (max(10 ** 6, n), max(10 ** 6, 5 * n)))

    def hk(d, n, s, mean=1000, small=False):
        holds, warm = (SMALL_HOLDS, SMALL_WARMUP) if small else hw(n)
        return (d, n, warm, holds, s, 0, mean)

    def sk(p, n, c, s):
        return (p, "exponential", n, n, M["sweep_steps"], c, s, 0, 1000)    # bench_sweep: warm-up defaults to N

    e = {f: {} for f in FILES}
    for n in M["ns"]:
        for d in DISTS:
            for s in M["seeds"]:
                for q in HA:
                    e["hold.csv"][(q, hk(d, n, s))] = "hold"
            for q in HA:
                e["hold_count.csv"][(q, hk(d, n, 1))] = "count_hold"
        for s in M["seeds"]:
            e["hold.csv"][("control", hk("exponential", n, s))] = "hold"
    for n in M["small_ns"]:
        for s in M["seeds"]:
            for q in ("heap", "avl", "oracle"):
                e["hold.csv"][(q, hk("exponential", n, s, small=True))] = "hold_small_n"
    for d in ("exponential", "bimodal"):
        for s in M["seeds"]:
            for q in HA:
                e["hold.csv"][(q, hk(d, M["sens_n"], s, mean=1000000))] = "hold_mean_us_1000000"
    for c in M["cs"]:
        for s in M["seeds"]:
            for q in HA:
                e["sweep.csv"][(q, sk("uniform", M["sweep_n"], c, s))] = "sweep"
        for q in HA:
            e["sweep_count.csv"][(q, sk("uniform", M["sweep_n"], c, 1))] = "count_sweep"
    for n in [M["sweep_n"]] + M["scale_ns"]:
        for p in ("latest", "uniform"):
            if p == "uniform" and n == M["sweep_n"]:
                continue
            for c in (0, 5000, 9500):
                for s in M["seeds"]:
                    for q in HA:
                        e["sweep.csv"][(q, sk(p, n, c, s))] = "sweep"
    return e


def cancels_for(bp, m):
    """bench_sweep's exact K = round(bp/10000 * m)."""
    return (m // 10000) * bp + ((m % 10000) * bp + 5000) // 10000


class Validator:
    def __init__(self):
        self.errors = []

    def fail(self, msg):
        self.errors.append(msg)

    def load(self, raw, name):
        cols, build, kind = FILES[name]
        path = os.path.join(raw, name)
        if not os.path.exists(path):
            self.fail(f"missing {path} (run scripts/run_bench.sh first)")
            return []
        with open(path, newline="") as f:
            reader = csv.DictReader(f)
            header = reader.fieldnames or []
            rows = list(reader)
        if header != cols:
            self.fail(f"{path}: header does not match CSV version {VERSION}")
            return []
        if not rows:
            self.fail(f"{path}: no data rows")
            return []
        good = []
        ops = "holds" if kind == "hold" else "steps"
        for i, r in enumerate(rows, start=2):
            where = f"{name}:{i}"
            if None in r or any(v is None for v in r.values()):
                self.fail(f"{where}: wrong number of fields")
                continue
            if r["version"] != VERSION or r["build"] != build or r["experiment"] != kind:
                self.fail(f"{where}: experiment/version/build '{r['experiment']}/{r['version']}/{r['build']}', "
                          f"expected {kind}/{VERSION}/{build}")
                continue
            ok = True
            for c in cols:
                if c in INT_COLS:
                    if not r[c].isdigit():
                        self.fail(f"{where}: {c}='{r[c]}' is not a non-negative integer")
                        ok = False
                    else:
                        r[c] = int(r[c])
                elif c in FLOAT_COLS:
                    try:
                        v = float(r[c])
                    except ValueError:
                        v = float("nan")
                    if not math.isfinite(v) or v < 0 or (c != "zero_inc_frac" and v <= 0):
                        self.fail(f"{where}: {c}='{r[c]}' is not a finite number in range")
                        ok = False
                    else:
                        r[c] = v
            if not ok:
                continue
            m, el = r[ops], r["elapsed_ns"]
            checks = [(r["n"] >= 1, "n must be >= 1"), (m >= 1, f"{ops} must be >= 1"), (el >= 1, "elapsed_ns must be >= 1")]
            if kind == "hold":
                checks.append((r["zero_inc_frac"] <= 1, "zero_inc_frac must be <= 1"))
            else:
                checks += [(r["cancel_bp"] <= 10000, "cancel_bp must be <= 10000"),
                           (r["cancels"] <= m, "cancels exceed steps"),
                           (r["cancels"] == cancels_for(r["cancel_bp"], m),
                            f"cancels={r['cancels']} but round({r['cancel_bp']}/10000*{m}) = {cancels_for(r['cancel_bp'], m)}")]
            if all(c for c, _ in checks):
                tput = m * 1e9 / el
                nsop = el / m
                checks += [(abs(r[f"{ops}_per_sec"] - tput) <= 0.051 + 1e-9 * tput, f"{ops}_per_sec does not match elapsed_ns"),
                           (abs(r[f"ns_per_{ops[:-1]}"] - nsop) <= 0.0006 + 1e-9 * nsop, f"ns_per_{ops[:-1]} does not match elapsed_ns")]
            bad = [msg for c, msg in checks if not c]
            if bad:
                self.fail(f"{where}: " + "; ".join(bad))
                continue
            r["tput"] = m * 1e9 / el               # exact, from integers
            r["ops"] = m
            r["key"] = hold_key(r) if kind == "hold" else sweep_key(r)
            r["where"] = where
            good.append(r)
        return good

    def check_file(self, name, rows, want):
        """Duplicates, exact matrix match (with settings), checksum agreement per workload."""
        seen = {}
        for r in rows:
            k = (r["queue"], r["key"])
            if k in seen:
                self.fail(f"{name}: duplicate run of {r['queue']} on workload {r['key']} ({seen[k]} and {r['where']})")
            else:
                seen[k] = r["where"]
            r["exp"] = want.get(k)
        missing = [k for k in want if k not in seen]
        extra = [(k, seen[k]) for k in seen if k not in want]
        if missing:
            self.fail(f"{name}: {len(missing)} run(s) of the matrix missing (queue, workload), e.g. {missing[:3]}")
        if extra:
            self.fail(f"{name}: {len(extra)} row(s) not in this mode's matrix — wrong settings (warm-up, length, "
                      f"mean, repeat…) or an experiment added to run_bench.sh but not to MATRIX in plot.py; "
                      f"e.g. {[w for _, w in extra[:3]]}")
        sums = defaultdict(dict)
        for r in rows:
            sums[r["key"]][r["queue"]] = r["checksum"]
        for k, v in sums.items():
            if len(set(v.values())) > 1:
                self.fail(f"{name}: different checksums on the same workload {k}: {v}")


def mean_sd(v):
    return st.mean(v), (st.stdev(v) if len(v) > 1 else 0.0), len(v)


def classify(ratios):
    heap = sum(1 for t in ratios if t > 1)
    avl = sum(1 for t in ratios if t < 1)
    tie = len(ratios) - heap - avl
    side = "heap" if heap == len(ratios) else "avl" if avl == len(ratios) else "tie" if tie == len(ratios) else "mixed"
    return heap, avl, tie, side


def clear_outputs(root):
    for p in [os.path.join(root, "results", "summary.csv"), os.path.join(root, "results", "paired_ratios.csv")] + \
             glob.glob(os.path.join(root, "results", "figures", "*.png")):
        if os.path.exists(p):
            os.remove(p)


def main(argv=None):
    ap = argparse.ArgumentParser(description="validate benchmark CSVs, then write summaries and figures")
    ap.add_argument("--mode", choices=tuple(MATRIX), default="full")
    ap.add_argument("--root", default=".")
    args = ap.parse_args(argv)
    mode, root = args.mode, args.root
    raw = os.path.join(root, "results", "raw")
    fig_dir = os.path.join(root, "results", "figures")
    tag = "" if mode == "full" else "  [QUICK — not for the report]" if mode == "quick" else "  [SELFTEST]"

    # ======================= validation: nothing is written before this completes =======================
    V = Validator()
    want = expected(mode)
    data = {}
    for name in FILES:
        data[name] = V.load(raw, name)
        if data[name]:
            V.check_file(name, data[name], want[name])
    paired = []                          # (experiment, key, heap row, avl row)
    for name in ("hold.csv", "sweep.csv"):
        by = defaultdict(dict)
        for r in data[name]:
            if r["queue"] in HA:
                by[r["key"]][r["queue"]] = r
        for k, v in sorted(by.items(), key=lambda kv: tuple(map(str, kv[0]))):
            if len(v) != 2:
                V.fail(f"{name}: workload {k} has {sorted(v)} but needs both heap and avl")
            else:
                paired.append((v["heap"]["exp"], k, v["heap"], v["avl"]))
    if V.errors:
        clear_outputs(root)
        for e in V.errors[:40]:
            print("plot.py: " + e, file=sys.stderr)
        if len(V.errors) > 40:
            print(f"plot.py: ... and {len(V.errors) - 40} more", file=sys.stderr)
        print("plot.py: validation FAILED — nothing written (stale summary/figures removed)", file=sys.stderr)
        return 1

    # ======================= outputs =======================
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    hold, sweep = data["hold.csv"], data["sweep.csv"]
    hold_count, sweep_count = data["hold_count.csv"], data["sweep_count.csv"]
    os.makedirs(fig_dir, exist_ok=True)
    summary = [("mode", "experiment", "queue", "dist_or_policy", "n", "x", "warmup", "ops", "mean_us",
                "mean_ops_per_sec", "sd_ops_per_sec", "runs", "note")]

    # aggregation over seeds/repeats only, within one validated setting (key minus seed and repeat)
    def setting(r):
        k = r["key"]
        return k[:4] + k[6:] if len(k) == 7 else k[:6] + k[8:]

    def series(rows, xf):
        g = defaultdict(list)
        for r in rows:
            g[(r["queue"], setting(r), xf(r))].append(r["tput"])
        return g

    def draw(ax, rows, xf, queues, label=lambda q: q):
        g = series(rows, xf)
        for q, style in queues:
            pts = sorted((x, v) for (qq, _, x), v in g.items() if qq == q)
            if pts:
                ms = [mean_sd(v) for _, v in pts]
                ax.errorbar([x for x, _ in pts], [m[0] for m in ms], yerr=[m[1] for m in ms], fmt=style,
                            capsize=3, label=label(q))

    for (q, sett, x), v in sorted(series(hold, lambda r: r["n"]).items(), key=lambda kv: tuple(map(str, kv[0]))):
        r0 = next(r for r in hold if r["queue"] == q and setting(r) == sett)
        m, s, k = mean_sd(v)
        note = f"zero_inc_frac={st.mean(r['zero_inc_frac'] for r in hold if r['queue'] == q and setting(r) == sett):.6f}"
        summary.append((mode, r0["exp"] if q != "control" else "hold_control", q, r0["dist"], r0["n"], "",
                        r0["warmup"], r0["holds"], r0["mean_us"], f"{m:.1f}", f"{s:.1f}", k, note))
    for (q, sett, x), v in sorted(series(sweep, lambda r: r["cancels"] / r["steps"]).items(), key=lambda kv: tuple(map(str, kv[0]))):
        r0 = next(r for r in sweep if r["queue"] == q and setting(r) == sett)
        m, s, k = mean_sd(v)
        summary.append((mode, "sweep_" + r0["policy"], q, r0["policy"], r0["n"], f"{x:.4f}", r0["warmup"], r0["steps"],
                        r0["mean_us"], f"{m:.1f}", f"{s:.1f}", k, f"cancel_bp={r0['cancel_bp']}"))

    # hold model vs N
    main_rows = [r for r in hold if r["exp"] == "hold"]
    fig, axes = plt.subplots(2, 2, figsize=(10, 7.5), sharex=True)
    for ax, d in zip(axes.flat, DISTS):
        draw(ax, [r for r in main_rows if r["dist"] == d], lambda r: r["n"],
             (("heap", "o-"), ("avl", "s-"), ("control", "k:")),
             lambda q: "control (no queue)" if q == "control" else q)
        ax.set_xscale("log")
        ax.set_yscale("log")
        ax.set_title(d)
        ax.set_ylabel("holds per second (mean ± sd)")
        ax.grid(alpha=0.3)
        ax.legend(fontsize=8)
    for ax in axes[-1]:
        ax.set_xlabel("queue size N")
    fig.suptitle("Hold model (Jones 1986): throughput, one hold = pop_min + insert" + tag)
    fig.tight_layout()
    fig.savefig(os.path.join(fig_dir, "hold_throughput_vs_n.png"), dpi=150)
    plt.close(fig)

    # small-N crossover
    fig, ax = plt.subplots(figsize=(6, 4.5))
    draw(ax, [r for r in hold if r["exp"] == "hold_small_n"], lambda r: r["n"],
         (("heap", "o-"), ("avl", "s-"), ("oracle", "^-")))
    ax.set_xscale("log")
    ax.set_xlabel("queue size N")
    ax.set_ylabel("holds per second")
    ax.set_title("Small queues: sorted array vs heap vs AVL (exponential)" + tag, fontsize=9)
    ax.grid(alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(os.path.join(fig_dir, "small_n_crossover.png"), dpi=150)
    plt.close(fig)

    # cancellation sweep
    M = MATRIX[mode]
    frac = lambda r: r["cancels"] / r["steps"]
    fig, ax = plt.subplots(figsize=(7, 4.5))
    draw(ax, [r for r in sweep if r["policy"] == "uniform" and r["n"] == M["sweep_n"]], frac, (("heap", "o-"), ("avl", "s-")))
    ax.set_xlabel("cancellation fraction of removals  K / M  (exact)")
    ax.set_ylabel("steps per second (1 removal + 1 insert)")
    ax.set_title(f"Cancellation sweep, N = {M['sweep_n']}, victim = uniform" + tag, fontsize=9)
    ax.grid(alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(os.path.join(fig_dir, "sweep_uniform.png"), dpi=150)
    plt.close(fig)

    fig, axes = plt.subplots(1, 2, figsize=(11, 4.2))
    for ax, pol in zip(axes, ("uniform", "latest")):
        for n in sorted({r["n"] for r in sweep}):
            draw(ax, [r for r in sweep if r["policy"] == pol and r["n"] == n], frac,
                 (("heap", "o-"), ("avl", "s-")), lambda q, n=n: f"{q} N={n}")
        ax.set_title(f"victim policy: {pol}" + tag, fontsize=9)
        ax.set_xlabel("K / M")
        ax.set_ylabel("steps per second")
        ax.set_yscale("log")
        ax.grid(alpha=0.3)
        ax.legend(fontsize=7)
    fig.tight_layout()
    fig.savefig(os.path.join(fig_dir, "sweep_policies.png"), dpi=150)
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(6.5, 4.2))
    for pol, style in (("uniform", "o-"), ("latest", "s--")):
        for n in sorted({r["n"] for r in sweep}):
            pts = defaultdict(list)
            for r in sweep:
                if r["queue"] == "heap" and r["policy"] == pol and r["n"] == n:
                    pts[frac(r)].append(r["peak_physical"] / r["n"])
            if pts:
                xs = sorted(pts)
                ax.plot(xs, [max(pts[x]) for x in xs], style, label=f"{pol} N={n}")
    ax.axhline(2.0, color="k", lw=0.8, ls=":")
    ax.set_xlabel("K / M")
    ax.set_ylabel("heap peak physical entries / live N (max over seeds)")
    ax.set_title("Lazy heap size stays within 2 x live + 1 (D1)" + tag, fontsize=9)
    ax.grid(alpha=0.3)
    ax.legend(fontsize=7)
    fig.tight_layout()
    fig.savefig(os.path.join(fig_dir, "heap_physical_ratio.png"), dpi=150)
    plt.close(fig)

    # operation counts (count build)
    fig, axes = plt.subplots(1, 3, figsize=(13, 4))
    for ax, col in zip(axes, ("comparisons", "moves", "hash_probes")):
        for q, style in (("heap", "o-"), ("avl", "s-")):
            pts = sorted((r["n"], r[col] / r["holds"]) for r in hold_count if r["queue"] == q and r["dist"] == "exponential")
            if pts and any(p[1] > 0 for p in pts):
                ax.plot([p[0] for p in pts], [p[1] for p in pts], style, label=q)
        ax.set_xscale("log")
        ax.set_title(f"{col} per hold (exponential)" + tag, fontsize=9)
        ax.set_xlabel("N")
        ax.grid(alpha=0.3)
        ax.legend()
    fig.tight_layout()
    fig.savefig(os.path.join(fig_dir, "counts_per_hold.png"), dpi=150)
    plt.close(fig)
    for r in hold_count:
        h = r["holds"]
        summary.append((mode, "count_hold", r["queue"], r["dist"], r["n"], "", r["warmup"], h, r["mean_us"], "", "", 1,
                        f"cmp={r['comparisons']/h:.2f};moves={r['moves']/h:.2f};probes={r['hash_probes']/h:.2f};"
                        f"rehash_probes={r['hash_rehash_probes']/h:.3f};"
                        f"rot={(r['rebalance_single'] + r['rebalance_double'])/h:.3f}"))
    for r in sweep_count:
        m = r["steps"]
        summary.append((mode, "count_sweep", r["queue"], r["policy"], r["n"], f"{r['cancels']/m:.4f}", r["warmup"], m,
                        r["mean_us"], "", "", 1,
                        f"cmp={r['comparisons']/m:.2f};probes={r['hash_probes']/m:.2f};"
                        f"rehash_probes={r['hash_rehash_probes']/m:.3f};purged={r['purged']/m:.3f};"
                        f"stale={r['stale_discards']/m:.3f}"))

    # paired ratios: same canonical key; grouped across seeds/repeats only within one setting
    pr = [("mode", "experiment", "workload_key", "heap_elapsed_ns", "avl_elapsed_ns", "ops", "heap_over_avl_throughput")]
    groups = defaultdict(list)
    for exp, k, h, a in paired:
        ratio = a["elapsed_ns"] / h["elapsed_ns"]          # same op count, so throughput ratio = time ratio
        pr.append((mode, exp, "|".join(map(str, k)), h["elapsed_ns"], a["elapsed_ns"], h["ops"], f"{ratio:.6f}"))
        groups[(exp, setting(h), h["dist"] if "policy" not in h else h["policy"], h["n"],
                "" if exp != "sweep" else f"{frac(h):.4f}")].append(ratio)
    for (exp, sett, a, n, x), v in sorted(groups.items(), key=lambda kv: tuple(map(str, kv[0]))):
        heap, avl, tie, side = classify(v)
        summary.append((mode, "ratio_" + exp, "heap/avl", a, n, x, "", "", "", f"{st.mean(v):.4f}",
                        f"{(st.stdev(v) if len(v) > 1 else 0):.4f}", len(v),
                        f"setting={'|'.join(map(str, sett))};min={min(v):.4f};max={max(v):.4f};"
                        f"heap_faster={heap};avl_faster={avl};ties={tie};all_same_side={side}"))

    with open(os.path.join(root, "results", "summary.csv"), "w", newline="") as f:
        csv.writer(f).writerows(summary)
    with open(os.path.join(root, "results", "paired_ratios.csv"), "w", newline="") as f:
        csv.writer(f).writerows(pr)
    print(f"plot.py ({mode}): validated {len(hold)} hold, {len(sweep)} sweep, {len(hold_count) + len(sweep_count)} "
          f"count rows; wrote results/summary.csv ({len(summary) - 1} rows), results/paired_ratios.csv "
          f"({len(pr) - 1} ratios), figures in results/figures/")
    return 0


if __name__ == "__main__":
    sys.exit(main())
