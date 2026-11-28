#!/usr/bin/env python3
"""Consolidate every Part 3 / Part 4 benchmark log, build the performance
model, and draw the final graphs.

Run from the repository root (Windows or Ubuntu):
    python part4/analysis/consolidate.py

Inputs (read only):
    part3/results/**        Part 3 benchmark logs
    part4/results/**        Part 4 benchmark logs (legacy folders, amer_batch,
                            campaign_* folders written by run_campaign.sh)
    part4/results/serial_summary.csv   per-host computation cost tc
    part0/results/network_summary.csv  measured latency and bandwidth

Outputs:
    part4/results/consolidated/*.csv
    part4/plots/*.png

Every parsed log appears in all_runs.csv with a status and reason, so nothing
is silently dropped. Logs with no solver summary are listed in
failed_attempts.csv.
"""

import csv
import math
import re
import statistics
from collections import defaultdict
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker

ROOT = Path(__file__).resolve().parents[2]
P3 = ROOT / "part3" / "results"
P4 = ROOT / "part4" / "results"
OUT = P4 / "consolidated"
PLOTS = ROOT / "part4" / "plots"

STEPS, R, CASE = 10, 0.24, "2"

IP_NAME = {
    "100.100.125.10": "heat-1",
    "100.98.24.109": "heat-2",
    "100.113.165.26": "heat-3",
}
HOSTFILES = {
    "hosts_all": [("heat-2", 2), ("heat-1", 2), ("heat-3", 2)],
    "hosts_ali": [("heat-2", 2), ("heat-1", 2)],
    "hosts_amer": [("heat-2", 2), ("heat-3", 2)],
    "hosts_local": [("heat-2", 2)],
}
WEAK_N = {1: 512, 2: 724, 3: 886, 4: 1023, 5: 1144, 6: 1253}
STRONG_N = (512, 2048, 8192)

# Runs kept on disk but excluded from medians, with the reason.
EXCLUDE_DIRS = {
    "strips_N8192_P1_slot_k7F1nW":
        "accidental second P=1 batch; kept as variability evidence",
}

SUMMARY_RE = re.compile(r"^decomp=(\w+) .*$", re.M)
FIELD_RE = re.compile(r"(\w+)=(\S+)")

# Colours: validated default categorical slots 1 and 2.
COLOR = {"strips": "#2a78d6", "blocks": "#eb6834"}
INK, INK2, GRID = "#0b0b0b", "#52514e", "#e4e3df"


# --------------------------------------------------------------------------
# Rank placement
# --------------------------------------------------------------------------

def place_ranks(hosts, P, mapping):
    """Return the host name of every rank (Open MPI slot / node mapping)."""
    free = [[name, slots] for name, slots in hosts]
    order = []
    if mapping == "slot":
        for entry in free:
            while entry[1] > 0 and len(order) < P:
                order.append(entry[0])
                entry[1] -= 1
    else:
        while len(order) < P and any(e[1] > 0 for e in free):
            for entry in free:
                if entry[1] > 0 and len(order) < P:
                    order.append(entry[0])
                    entry[1] -= 1
    if len(order) < P:
        return None
    return order


def placement_label(ranks):
    counts = defaultdict(int)
    for h in ranks:
        counts[h] += 1
    seen = []
    for h in ranks:
        if h not in seen:
            seen.append(h)
    return "+".join(f"{h}x{counts[h]}" for h in seen)


def hosts_from_meta(meta_path):
    hosts, in_hf = [], False
    for line in meta_path.read_text(errors="replace").splitlines():
        if line.startswith("--- hostfile"):
            in_hf = True
            continue
        if in_hf and line.strip() and not line.lstrip().startswith("#"):
            parts = line.split()
            slots = 1
            for p in parts[1:]:
                if p.startswith("slots="):
                    slots = int(p.split("=")[1])
            hosts.append((IP_NAME.get(parts[0], parts[0]), slots))
    meta = dict(
        l.split("=", 1) for l in meta_path.read_text(errors="replace").splitlines()
        if "=" in l and not l.startswith("---") and not in_hf_line(l)
    )
    return hosts, meta


def in_hf_line(line):
    return bool(re.match(r"^\s*\d+\.\d+\.\d+\.\d+", line))


# --------------------------------------------------------------------------
# Log discovery and classification
# --------------------------------------------------------------------------

def classify(path):
    """Return (source, hostfile_hosts, mapping, status, reason)."""
    rel = path.relative_to(ROOT).as_posix()
    parts = path.relative_to(ROOT).parts

    if "consolidated" in parts or path.name == "campaign.log":
        return None
    if any(p.startswith("serial_") for p in parts):
        return None  # serial baseline is handled by serial_summary.csv

    # New campaign folders: placement is recorded in meta.txt.
    for i, p in enumerate(parts):
        if p.startswith("campaign_"):
            meta_path = path.parent / "meta.txt"
            if not meta_path.exists():
                return (rel, None, None, "excluded", "campaign log without meta.txt")
            hosts, meta = hosts_from_meta(meta_path)
            btl = meta.get("btl", "?")
            src = f"{parts[0]}/{p}"
            if btl != "tcp,self":
                return (src, hosts, meta.get("map", "slot"), "excluded",
                        f"different MPI transport ({btl})")
            return (src, hosts, meta.get("map", "slot"), "ok", "")

    if parts[0] == "part4":
        if "lan_pilot" in parts:
            return ("part4/lan_pilot", None, None, "excluded",
                    "direct-LAN diagnostic, not a Tailscale measurement")
        if "amer_batch" in parts:
            m = re.search(r"_(slot|node)$", path.parent.name)
            if path.name.startswith("attempt_"):
                return ("part4/amer_batch", HOSTFILES["hosts_amer"],
                        m.group(1) if m else "slot", "excluded",
                        "unfinished attempt file")
            return ("part4/amer_batch", HOSTFILES["hosts_amer"],
                    m.group(1) if m else "slot", "ok", "")
        d = path.parent.name
        m = re.match(r"(strips|blocks)_N\d+_P\d+_(slot|node)", d)
        if m:
            if d in EXCLUDE_DIRS:
                return ("part4/legacy", HOSTFILES["hosts_all"], m.group(2),
                        "excluded", EXCLUDE_DIRS[d])
            return ("part4/legacy", HOSTFILES["hosts_all"], m.group(2), "ok", "")
        return None

    if parts[0] == "part3":
        d = path.parent.name
        if d == "final_benchmarks":
            return ("part3/final_benchmarks", HOSTFILES["hosts_amer"], "slot",
                    "ok", "")
        if d.startswith("benchmark_full_"):
            # Same 12 runs as final_benchmarks; duplicates are removed later.
            return ("part3/benchmark_full", HOSTFILES["hosts_amer"], "slot",
                    "ok", "")
        if d.startswith("benchmark_pilot_"):
            return ("part3/pilot", None, "slot", "excluded",
                    "pilot run (machine pair not recorded)")
        return ("part3/other", None, None, "excluded",
                "correctness or demo run, not a benchmark")
    return None


def scan():
    runs, failures = [], []
    files = sorted(list(P3.rglob("*.log")) + list(P4.rglob("*.log"))
                   + list(P4.rglob("attempt_*")))
    seen_files = set()
    for path in files:
        if path in seen_files or not path.is_file():
            continue
        seen_files.add(path)
        info = classify(path)
        if info is None:
            continue
        source, hosts, mapping, status, reason = info
        text = path.read_text(errors="replace")
        rel = path.relative_to(ROOT).as_posix()
        m = SUMMARY_RE.search(text)
        if not m:
            last = text.strip().splitlines()[-1] if text.strip() else "(empty)"
            failures.append({"file": rel, "source": source,
                             "bytes": path.stat().st_size, "last_line": last})
            continue
        f = dict(FIELD_RE.findall(m.group(0)))
        if f.get("decomp") not in ("strips", "blocks"):
            continue
        run = {
            "file": rel, "source": source, "summary": m.group(0).strip(),
            "method": f["decomp"], "N": int(f["N"]), "P": int(f["P"]),
            "case": f.get("case"), "r": float(f.get("r", "nan")),
            "steps": int(f.get("steps", -1)),
            "t_total": float(f["t_total"]), "t_comp": float(f["t_comp"]),
            "t_comm": float(f["t_comm"]),
            "map": mapping or "", "placement": "", "status": status,
            "reason": reason,
        }
        if hosts:
            ranks = place_ranks(hosts, run["P"], mapping or "slot")
            run["placement"] = placement_label(ranks) if ranks else "?"
        if status == "ok" and (run["case"] != CASE or run["steps"] != STEPS
                               or abs(run["r"] - R) > 1e-12):
            run["status"], run["reason"] = "excluded", "non-standard parameters"
        runs.append(run)

    # Remove exact duplicates (same summary line copied into two folders).
    first = {}
    for run in runs:
        if run["status"] != "ok":
            continue
        key = run["summary"]
        if key in first:
            run["status"] = "excluded"
            run["reason"] = f"duplicate of {first[key]}"
        else:
            first[key] = run["file"]
    return runs, failures


# --------------------------------------------------------------------------
# Statistics
# --------------------------------------------------------------------------

def summarize(runs):
    groups = defaultdict(list)
    for r in runs:
        if r["status"] == "ok":
            groups[(r["method"], r["N"], r["P"], r["map"], r["placement"])].append(r)
    rows = []
    for (method, N, P, mp, pl), rs in sorted(groups.items()):
        tt = [r["t_total"] for r in rs]
        share = [100 * r["t_comm"] / r["t_total"] for r in rs if r["t_total"] > 0]
        rows.append({
            "method": method, "N": N, "P": P, "map": mp, "placement": pl,
            "n_runs": len(rs),
            "median_t_total": statistics.median(tt),
            "min_t_total": min(tt), "max_t_total": max(tt),
            "median_t_comp": statistics.median(r["t_comp"] for r in rs),
            "median_t_comm": statistics.median(r["t_comm"] for r in rs),
            "median_comm_share_pct": statistics.median(share) if share else "",
            "sources": ";".join(sorted({r["source"] for r in rs})),
        })
    return rows


def lookup(summary, method, N, P, mp, placement=None):
    cands = [s for s in summary if s["method"] == method and s["N"] == N
             and s["P"] == P and s["map"] == mp
             and (placement is None or s["placement"] == placement)]
    if not cands:
        return None
    return max(cands, key=lambda s: s["n_runs"])


def all_placement(P, mp="slot"):
    return placement_label(place_ranks(HOSTFILES["hosts_all"], P, mp))


# --------------------------------------------------------------------------
# Performance model
# --------------------------------------------------------------------------

def load_tc():
    tc = defaultdict(dict)
    with open(P4 / "serial_summary.csv", newline="") as fh:
        for row in csv.DictReader(fh):
            tc[row["host"]][int(row["N"])] = float(row["tc_seconds"])
    return tc


def tc_for(tc, host, N):
    table = tc[host]
    nearest = min(table, key=lambda n: abs(math.log2(n) - math.log2(N)))
    return table[nearest]


def load_net():
    net = {}
    with open(ROOT / "part0" / "results" / "network_summary.csv", newline="") as fh:
        for row in csv.DictReader(fh):
            ts = float(row["latency_ms"]) / 1000.0
            tw = 8.0 / (float(row["bandwidth_MB_s"]) * 1e6)
            c = row["connection"]
            if c.startswith("same_"):
                h = "heat-" + c[len("same_heat"):]
                net[(h, h)] = (ts, tw)
            else:
                a, b = re.findall(r"heat(\d)", c)
                net[(f"heat-{a}", f"heat-{b}")] = (ts, tw)
                net[(f"heat-{b}", f"heat-{a}")] = (ts, tw)
    return net


def split(total, parts, coord):
    base, rem = divmod(total, parts)
    return base + (1 if coord < rem else 0)


DIMS = {1: (1, 1), 2: (2, 1), 3: (3, 1), 4: (2, 2), 5: (5, 1), 6: (3, 2),
        8: (4, 2), 9: (3, 3), 12: (4, 3), 16: (4, 4)}


def predict(method, N, ranks, tc, net, steps=STEPS):
    """Predicted (t_total, t_comp, t_comm) for one configuration.

    Per step and per rank: compute = owned points * tc(host); each
    MPI_Sendrecv costs ts + m*tw of the slower of its two links. The step
    time is the slowest rank, multiplied by the number of steps.
    """
    P = len(ranks)
    worst, worst_comp, worst_comm = 0.0, 0.0, 0.0

    def link(a, b, m):
        ts, tw = net[(ranks[a], ranks[b])]
        return ts + m * tw

    for rk in range(P):
        if method == "strips":
            rows = split(N - 1, P, rk)
            points = rows * (N - 1)
            m = N + 1
            below = rk - 1 if rk > 0 else None
            above = rk + 1 if rk < P - 1 else None
            calls = [[above, below], [below, above]]
            msg = [m, m]
        else:
            py, px = DIMS[P]
            cy, cx = divmod(rk, px)
            ny = split(N - 1, py, cy)
            nx = split(N - 1, px, cx)
            points = nx * ny
            up = rk + px if cy < py - 1 else None
            down = rk - px if cy > 0 else None
            right = rk + 1 if cx < px - 1 else None
            left = rk - 1 if cx > 0 else None
            calls = [[up, down], [down, up], [right, left], [left, right]]
            msg = [nx, nx, ny, ny]
        comp = points * tc_for(tc, ranks[rk], N)
        comm = 0.0
        for (dst, src), m in zip(calls, msg):
            costs = [link(rk, o, m) for o in (dst, src) if o is not None]
            if costs:
                comm += max(costs)
        if comp + comm > worst:
            worst, worst_comp, worst_comm = comp + comm, comp, comm
    return steps * worst, steps * worst_comp, steps * worst_comm


# --------------------------------------------------------------------------
# Output helpers
# --------------------------------------------------------------------------

def write_csv(name, rows, fields=None):
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / name
    if not rows:
        path.write_text("(no rows)\n")
        return
    fields = fields or list(rows[0].keys())
    with open(path, "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=fields, extrasaction="ignore")
        w.writeheader()
        w.writerows(rows)


def style(ax, title, xlabel, ylabel):
    ax.set_title(title, color=INK, fontsize=11, loc="left")
    ax.set_xlabel(xlabel, color=INK2)
    ax.set_ylabel(ylabel, color=INK2)
    ax.grid(True, color=GRID, linewidth=0.8)
    ax.set_axisbelow(True)
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)
    for s in ("left", "bottom"):
        ax.spines[s].set_color(INK2)
    ax.tick_params(colors=INK2)
    fmt = matplotlib.ticker.FuncFormatter(lambda v, _: f"{v:g}")
    for axis, scale in ((ax.yaxis, ax.get_yscale()), (ax.xaxis, ax.get_xscale())):
        if scale == "log":
            axis.set_major_formatter(fmt)
            axis.set_minor_formatter(matplotlib.ticker.NullFormatter())


def save(fig, name):
    PLOTS.mkdir(parents=True, exist_ok=True)
    if not getattr(fig, "_laid_out", False):
        fig.tight_layout()
    fig.savefig(PLOTS / name, dpi=160)
    plt.close(fig)


# --------------------------------------------------------------------------
# Main
# --------------------------------------------------------------------------

def main():
    runs, failures = scan()
    summary = summarize(runs)
    tc, net = load_tc(), load_net()

    write_csv("all_runs.csv", runs, [
        "method", "N", "P", "map", "placement", "t_total", "t_comp", "t_comm",
        "status", "reason", "source", "file"])
    write_csv("failed_attempts.csv", failures)
    write_csv("summary.csv", summary)

    # Model vs measurement for every measured group with a known placement.
    model_rows = []
    for s in summary:
        if not s["placement"] or s["placement"] == "?":
            continue
        ranks = []
        for part in s["placement"].split("+"):
            h, k = part.rsplit("x", 1)
            ranks += [h] * int(k)
        if s["map"] == "node":
            hosts = [(h, int(k)) for h, k in
                     (p.rsplit("x", 1) for p in s["placement"].split("+"))]
            # Rebuild round-robin order from the label's host order.
            ranks = place_ranks([(h, 2) for h, _ in hosts], s["P"], "node")
        pt, pc, pm = predict(s["method"], s["N"], ranks, tc, net)
        model_rows.append({
            "method": s["method"], "N": s["N"], "P": s["P"], "map": s["map"],
            "placement": s["placement"], "n_runs": s["n_runs"],
            "measured_median_s": s["median_t_total"], "predicted_s": pt,
            "predicted_comp_s": pc, "predicted_comm_s": pm,
            "measured_over_predicted": s["median_t_total"] / pt if pt else "",
        })
    write_csv("model_vs_measured.csv", model_rows)

    # Strong scaling (slot placement on hosts_all; P=1 is each method's baseline).
    strong = []
    for N in STRONG_N:
        for method in ("strips", "blocks"):
            base = lookup(summary, method, N, 1, "slot", all_placement(1))
            for P in range(1, 7):
                s = lookup(summary, method, N, P, "slot", all_placement(P))
                ranks = place_ranks(HOSTFILES["hosts_all"], P, "slot")
                pt = predict(method, N, ranks, tc, net)[0]
                row = {"method": method, "N": N, "P": P,
                       "placement": all_placement(P), "predicted_s": pt,
                       "n_runs": s["n_runs"] if s else 0,
                       "median_s": s["median_t_total"] if s else "",
                       "min_s": s["min_t_total"] if s else "",
                       "max_s": s["max_t_total"] if s else "",
                       "comm_share_pct": s["median_comm_share_pct"] if s else "",
                       "speedup": "", "efficiency_pct": "",
                       "predicted_speedup": ""}
                if base:
                    row["predicted_speedup"] = predict(
                        method, N, ["heat-2"], tc, net)[0] / pt
                if s and base:
                    sp = base["median_t_total"] / s["median_t_total"]
                    row["speedup"], row["efficiency_pct"] = sp, 100 * sp / P
                strong.append(row)
    write_csv("strong_scaling.csv", strong)

    # Weak scaling.
    weak = []
    for method in ("strips", "blocks"):
        base = lookup(summary, method, 512, 1, "slot", all_placement(1))
        for P, N in WEAK_N.items():
            s = lookup(summary, method, N, P, "slot", all_placement(P))
            note = ""
            if s is None:
                s = lookup(summary, method, N, P, "slot")
                if s:
                    note = f"different machines ({s['placement']})"
            row = {"method": method, "P": P, "N": N,
                   "placement": s["placement"] if s else all_placement(P),
                   "n_runs": s["n_runs"] if s else 0,
                   "median_s": s["median_t_total"] if s else "",
                   "weak_efficiency_pct": "", "note": note}
            if s and base:
                row["weak_efficiency_pct"] = 100 * base["median_t_total"] / s["median_t_total"]
            weak.append(row)
    write_csv("weak_scaling.csv", weak)

    # Crossover: when does P=4 across heat-2 + heat-1 beat P=2 inside heat-2?
    cross = []
    sizes = [2 ** k for k in range(8, 16)]
    for method in ("strips", "blocks"):
        for N in sizes:
            local = predict(method, N, ["heat-2", "heat-2"], tc, net)[0]
            dist4 = predict(method, N, ["heat-2", "heat-2", "heat-1", "heat-1"], tc, net)[0]
            dist6 = predict(method, N, place_ranks(HOSTFILES["hosts_all"], 6, "slot"), tc, net)[0]
            cross.append({"method": method, "N": N,
                          "pred_P2_local_s": local, "pred_P4_two_vms_s": dist4,
                          "pred_P6_three_vms_s": dist6,
                          "P4_beats_local": dist4 < local,
                          "P6_beats_local": dist6 < local})
    write_csv("model_crossover.csv", cross)

    # Hypothetical 1 microsecond latency (bandwidth and tc unchanged).
    fast = {k: (1e-6 if k[0] != k[1] else v[0], v[1]) for k, v in net.items()}
    hypo = []
    for method in ("strips", "blocks"):
        for N in STRONG_N:
            for P in (2, 4, 6):
                ranks = place_ranks(HOSTFILES["hosts_all"], P, "slot")
                hypo.append({"method": method, "N": N, "P": P,
                             "placement": placement_label(ranks),
                             "pred_measured_network_s": predict(method, N, ranks, tc, net)[0],
                             "pred_1us_latency_s": predict(method, N, ranks, tc, fast)[0]})
    write_csv("model_1us_latency.csv", hypo)

    draw(summary, strong, weak, model_rows, cross)
    report(runs, failures, summary, strong, weak, cross)


# --------------------------------------------------------------------------
# Plots
# --------------------------------------------------------------------------

def draw(summary, strong, weak, model_rows, cross):
    # 1. Strong-scaling speedup per grid size.
    fig, axes = plt.subplots(1, 3, figsize=(13, 4), sharey=False)
    for ax, N in zip(axes, STRONG_N):
        for method in ("strips", "blocks"):
            pts = [(r["P"], r["speedup"]) for r in strong
                   if r["N"] == N and r["method"] == method and r["speedup"] != ""]
            if pts:
                ax.plot(*zip(*pts), marker="o", markersize=6, linewidth=2,
                        color=COLOR[method], label=f"{method} measured")
            pred = [(r["P"], r["predicted_speedup"]) for r in strong
                    if r["N"] == N and r["method"] == method and r["predicted_speedup"] != ""]
            if pred:
                ax.plot(*zip(*pred), linestyle="--", linewidth=1.5,
                        color=COLOR[method], alpha=0.7, label=f"{method} model")
        ax.plot([1, 6], [1, 6], color=INK2, linewidth=1, linestyle=":", label="ideal")
        if N == 8192:
            style(ax, f"N = {N}", "processes P", "speed-up T1/TP")
        else:
            ax.set_yscale("log")
            style(ax, f"N = {N}", "processes P", "speed-up T1/TP (log)")
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, frameon=False, fontsize=9, ncol=5,
               loc="lower center")
    fig.suptitle("Strong scaling, slot placement (heat-2, then heat-1, then heat-3)",
                 color=INK, fontsize=12, x=0.01, ha="left")
    fig.tight_layout(rect=(0, 0.07, 1, 1))
    fig._laid_out = True
    save(fig, "strong_scaling_speedup.png")

    # 2. Strong-scaling time with min/max range.
    fig, axes = plt.subplots(1, 3, figsize=(13, 4))
    for ax, N in zip(axes, STRONG_N):
        for method in ("strips", "blocks"):
            rows = [r for r in strong if r["N"] == N and r["method"] == method
                    and r["median_s"] != ""]
            if rows:
                P = [r["P"] for r in rows]
                med = [r["median_s"] for r in rows]
                lo = [r["median_s"] - r["min_s"] for r in rows]
                hi = [r["max_s"] - r["median_s"] for r in rows]
                ax.errorbar(P, med, yerr=[lo, hi], marker="o", markersize=6,
                            linewidth=2, capsize=3, color=COLOR[method], label=method)
        ax.set_yscale("log")
        style(ax, f"N = {N}", "processes P", "time for 10 steps, s (log)")
    axes[0].legend(frameon=False, fontsize=8)
    fig.suptitle("Measured time: median, bars show min to max",
                 color=INK, fontsize=12, x=0.01, ha="left")
    save(fig, "strong_scaling_time.png")

    # 3. Weak-scaling efficiency.
    fig, ax = plt.subplots(figsize=(6.5, 4))
    for method in ("strips", "blocks"):
        pts = [(r["P"], r["weak_efficiency_pct"]) for r in weak
               if r["method"] == method and r["weak_efficiency_pct"] != ""]
        if pts:
            ax.plot(*zip(*pts), marker="o", markersize=6, linewidth=2,
                    color=COLOR[method], label=method)
    ax.axhline(100, color=INK2, linewidth=1, linestyle=":")
    ax.set_yscale("log")
    style(ax, "Weak scaling efficiency T1/TP (511² points per process)",
          "processes P", "efficiency, % (log)")
    ax.legend(frameon=False)
    save(fig, "weak_scaling_efficiency.png")

    # 4. Communication share for every parallel configuration.
    rows = [s for s in summary if s["P"] > 1 and s["median_comm_share_pct"] != ""
            and s["map"] == "slot"]
    rows.sort(key=lambda s: (s["N"], s["P"], s["method"]))
    if rows:
        fig, ax = plt.subplots(figsize=(max(7, 0.38 * len(rows)), 4.2))
        x = range(len(rows))
        ax.bar(x, [s["median_comm_share_pct"] for s in rows], width=0.8,
               color=[COLOR[s["method"]] for s in rows], edgecolor="white",
               linewidth=2)
        ax.set_xticks(list(x))
        ax.set_xticklabels([f"{s['method'][0].upper()} N{s['N']} P{s['P']}\n{s['placement']}"
                            for s in rows], rotation=90, fontsize=6.5)
        ax.set_ylim(0, 100)
        style(ax, "Communication share t_comm / t_total (median)", "", "%")
        from matplotlib.patches import Patch
        ax.legend(handles=[Patch(color=COLOR[m], label=m) for m in COLOR],
                  frameon=False, loc="lower right")
        save(fig, "communication_share.png")

    # 5. Part 3: strips vs blocks at P=4, per machine pair.
    p4 = [s for s in summary if s["P"] == 4 and s["map"] == "slot"]
    pairs = sorted({s["placement"] for s in p4})
    if p4:
        fig, axes = plt.subplots(1, len(pairs), figsize=(5.5 * len(pairs), 4),
                                 squeeze=False)
        for ax, pl in zip(axes[0], pairs):
            Ns = sorted({s["N"] for s in p4 if s["placement"] == pl})
            for k, method in enumerate(("strips", "blocks")):
                xs, med, lo, hi = [], [], [], []
                for i, N in enumerate(Ns):
                    s = lookup(p4, method, N, 4, "slot", pl)
                    if s:
                        xs.append(i + (k - 0.5) * 0.36)
                        med.append(s["median_t_total"])
                        lo.append(s["median_t_total"] - s["min_t_total"])
                        hi.append(s["max_t_total"] - s["median_t_total"])
                ax.bar(xs, med, width=0.34, color=COLOR[method], label=method,
                       yerr=[lo, hi], capsize=3, edgecolor="white", linewidth=2,
                       error_kw={"ecolor": INK2, "linewidth": 1})
            ax.set_xticks(range(len(Ns)))
            ax.set_xticklabels([f"N={N}" for N in Ns])
            style(ax, f"P = 4 on {pl}", "", "time for 10 steps, s")
        axes[0][0].legend(frameon=False)
        save(fig, "part3_strips_vs_blocks_P4.png")

    # 6. Model vs measured.
    if model_rows:
        fig, ax = plt.subplots(figsize=(5.5, 5))
        for method in ("strips", "blocks"):
            pts = [(r["predicted_s"], r["measured_median_s"]) for r in model_rows
                   if r["method"] == method and r["P"] > 1]
            if pts:
                ax.scatter(*zip(*pts), s=40, color=COLOR[method], label=method,
                           edgecolor="white", linewidth=1.5, zorder=3)
        lo = min(min(r["predicted_s"], r["measured_median_s"]) for r in model_rows)
        hi = max(max(r["predicted_s"], r["measured_median_s"]) for r in model_rows)
        ax.plot([lo, hi], [lo, hi], color=INK2, linestyle=":", linewidth=1,
                label="measured = model")
        ax.set_xscale("log")
        ax.set_yscale("log")
        style(ax, "Model vs measured (P > 1)", "predicted, s", "measured median, s")
        ax.legend(frameon=False, fontsize=8)
        save(fig, "model_vs_measured.png")

    # 7. Predicted crossover.
    fig, axes = plt.subplots(1, 2, figsize=(11, 4))
    for ax, method in zip(axes, ("strips", "blocks")):
        rows = [r for r in cross if r["method"] == method]
        Ns = [r["N"] for r in rows]
        ax.plot(Ns, [r["pred_P2_local_s"] for r in rows], marker="o", linewidth=2,
                color=INK2, label="P=2, one VM")
        ax.plot(Ns, [r["pred_P4_two_vms_s"] for r in rows], marker="o", linewidth=2,
                color=COLOR[method], label="P=4, two VMs")
        ax.plot(Ns, [r["pred_P6_three_vms_s"] for r in rows], marker="s", linewidth=2,
                linestyle="--", color=COLOR[method], label="P=6, three VMs")
        ax.set_xscale("log", base=2)
        ax.set_yscale("log")
        style(ax, f"Model: {method}", "grid size N", "predicted time, 10 steps, s")
        ax.legend(frameon=False, fontsize=8)
    save(fig, "model_crossover.png")


def report(runs, failures, summary, strong, weak, cross):
    ok = sum(r["status"] == "ok" for r in runs)
    print(f"Parsed {len(runs)} benchmark logs: {ok} used, {len(runs) - ok} excluded.")
    print(f"Logs without a solver summary (failed or killed): {len(failures)}")
    print("\nStrong scaling (slot, hosts_all): runs per configuration")
    for N in STRONG_N:
        for method in ("strips", "blocks"):
            cells = [str(r["n_runs"]) for r in strong
                     if r["N"] == N and r["method"] == method]
            print(f"  N={N:<5} {method:<6} P1..P6: {' '.join(cells)}")
    print("\nWeak scaling: runs per configuration")
    for method in ("strips", "blocks"):
        cells = [str(r["n_runs"]) + ("*" if r["note"] else "") for r in weak
                 if r["method"] == method]
        print(f"  {method:<6} P1..P6: {' '.join(cells)}   (* = other machines)")
    for method in ("strips", "blocks"):
        hit = [r["N"] for r in cross if r["method"] == method and r["P4_beats_local"]]
        print(f"\nModel crossover {method}: P=4 on two VMs beats P=2 local from N = "
              f"{hit[0] if hit else 'none up to 32768'}")
    print(f"\nWrote CSV files to {OUT.relative_to(ROOT)} and graphs to "
          f"{PLOTS.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
