#!/usr/bin/env python3
"""Part 3 question: for the measured ts, tw and tc, should strips or blocks be
faster for P = 4, 8 and N = 512, 2048, 8192? Then check against measurements.

Two models, both from Task 8.2 of the background notes:
  simple : one interior process on the cross-VM link,
           strips Tcomm = 2(ts + (N+1) tw), blocks Tcomm = 4(ts + (N/sqrt(P)) tw)
  refined: every rank on its real VM (slot placement), real block sizes,
           physical edges, same-VM vs cross-VM links, per-VM compute speed
           (predict() in part4/analysis/consolidate.py).

Setup: heat-2 + heat-3 on the 28 Nov phone hotspot (direct Tailscale path),
network parameters from part0/results/raw/heat2_heat3_hotspot.csv.
P = 4 is heat-2 x2 + heat-3 x2 (measured). P = 8 is heat-2 x4 + heat-3 x4
(hypothetical: needs 8 vCPUs, not measured).

Run from the repo root:  python part3/analysis/predict_strips_vs_blocks.py
"""
import csv
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "part4" / "analysis"))
import consolidate as m  # noqa: E402

tc = m.load_tc()
net = m.load_nets()[m.HOTSPOT]
ts_x, tw_x = net[("heat-2", "heat-3")]
PLACE = {4: ["heat-2"] * 2 + ["heat-3"] * 2, 8: ["heat-2"] * 4 + ["heat-3"] * 4}

# Measured medians (10 steps) for the same setup, from the consolidated summary.
measured = {}
with open(ROOT / "part4" / "results" / "consolidated" / "summary.csv", newline="") as fh:
    for r in csv.DictReader(fh):
        if (r["network"] == m.HOTSPOT and r["placement"] == "heat-2x2+heat-3x2"
                and r["P"] == "4" and r["map"] == "slot"):
            measured[(r["method"], int(r["N"]))] = float(r["median_t_total"])

rows = []
for P in (4, 8):
    for N in (512, 2048, 8192):
        s_simple = 2 * (ts_x + (N + 1) * tw_x)
        b_simple = 4 * (ts_x + (N / math.sqrt(P)) * tw_x)
        s_step = m.predict("strips", N, PLACE[P], tc, net, steps=1)[0]
        b_step = m.predict("blocks", N, PLACE[P], tc, net, steps=1)[0]
        ms, mb = measured.get(("strips", N)), measured.get(("blocks", N))
        rows.append({
            "P": P, "N": N,
            "simple_strips_comm_ms": round(1e3 * s_simple, 2),
            "simple_blocks_comm_ms": round(1e3 * b_simple, 2),
            "refined_strips_10steps_s": round(10 * s_step, 3),
            "refined_blocks_10steps_s": round(10 * b_step, 3),
            "simple_predicts": "blocks" if b_simple < s_simple else "strips",
            "refined_predicts": "blocks" if b_step < s_step else "strips",
            "measured_strips_s": round(ms, 3) if ms and P == 4 else "",
            "measured_blocks_s": round(mb, 3) if mb and P == 4 else "",
            "measured_faster": ("blocks" if mb < ms else "strips") if ms and mb and P == 4 else "",
        })

out = ROOT / "part3" / "results" / "predictions_strips_vs_blocks.csv"
with open(out, "w", newline="") as fh:
    w = csv.DictWriter(fh, fieldnames=list(rows[0]))
    w.writeheader()
    w.writerows(rows)
print(f"heat-2 <-> heat-3, hotspot: ts = {ts_x * 1e3:.3f} ms, "
      f"bandwidth = {8 / tw_x / 1e6:.3f} MB/s")
for r in rows:
    print(r)
print("wrote", out.relative_to(ROOT))
