#!/usr/bin/env python3
"""Part 3 question: for the measured ts, tw and tc, should strips or blocks be
faster for P = 4, 8 and N = 512, 2048, 8192?

Two models, both from Task 8.2 of the background notes:
  simple : one interior process on the slowest cross-VM link,
           strips Tcomm = 2(ts + (N+1) tw), blocks Tcomm = 4(ts + (N/sqrt(P)) tw)
  refined: every rank on its real VM (slot placement), real block sizes,
           physical edges, same-VM vs cross-VM links, per-VM compute speed
           (the model in part4/analysis/consolidate.py).
Placements: P=4 = heat-2 x2 + heat-1 x2 (hosts_ali);
            P=8 = heat-2 x4 + heat-1 x4 (hypothetical 8-vCPU option A).
Run from the repo root:  python part3/analysis/predict_strips_vs_blocks.py
"""
import csv, math, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "part4" / "analysis"))
import consolidate as m  # noqa: E402

tc, net = m.load_tc(), m.load_net()
ts_x, tw_x = net[("heat-2", "heat-1")]
PLACE = {4: ["heat-2"] * 2 + ["heat-1"] * 2, 8: ["heat-2"] * 4 + ["heat-1"] * 4}
rows = []
for P in (4, 8):
    for N in (512, 2048, 8192):
        s_simple = 2 * (ts_x + (N + 1) * tw_x)
        b_simple = 4 * (ts_x + (N / math.sqrt(P)) * tw_x)
        s_tot, _, s_comm = m.predict("strips", N, PLACE[P], tc, net, steps=1)
        b_tot, _, b_comm = m.predict("blocks", N, PLACE[P], tc, net, steps=1)
        rows.append({
            "P": P, "N": N,
            "simple_strips_comm_ms": round(1e3 * s_simple, 3),
            "simple_blocks_comm_ms": round(1e3 * b_simple, 3),
            "refined_strips_step_ms": round(1e3 * s_tot, 3),
            "refined_blocks_step_ms": round(1e3 * b_tot, 3),
            "predicted_faster": "blocks" if b_tot < s_tot else "strips",
        })
out = ROOT / "part3" / "results" / "predictions_strips_vs_blocks.csv"
with open(out, "w", newline="") as fh:
    w = csv.DictWriter(fh, fieldnames=list(rows[0]))
    w.writeheader(); w.writerows(rows)
print(f"ts={ts_x*1e3:.3f} ms  bandwidth={8/tw_x/1e6:.3f} MB/s (heat-2 <-> heat-1)")
for r in rows: print(r)
print("wrote", out.relative_to(ROOT))
