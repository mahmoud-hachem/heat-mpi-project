import csv
import math
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

results = Path("results")
rows = []

# Read the actual output of the five solver runs
for line in (results / "convergence_runs.txt").read_text().splitlines():
    if not line.startswith("decomp=serial"):
        continue

    values = dict(item.split("=", 1) for item in line.split() if "=" in item)
    N = int(values["N"])

    rows.append({
        "N": N,
        "h": 1.0 / N,
        "steps": int(values["steps"]),
        "max_error": float(values["err_inf"]),
        "l2h_error": float(values["err_l2h"]),
    })

rows.sort(key=lambda row: row["N"])

assert [row["N"] for row in rows] == [16, 32, 64, 128, 256]

# Calculate observed order of convergence
for i, row in enumerate(rows):
    row["p_obs"] = "" if i == 0 else math.log2(
        rows[i-1]["l2h_error"] / row["l2h_error"]
    )

# Save the completed table
with (results / "convergence.csv").open("w", newline="") as file:
    writer = csv.DictWriter(file, fieldnames=rows[0].keys())
    writer.writeheader()
    writer.writerows(rows)

# Plot measured error and theoretical second-order reference
h = [row["h"] for row in rows]
errors = [row["l2h_error"] for row in rows]
reference = [errors[0] * (x / h[0])**2 for x in h]

plt.figure(figsize=(7, 5))
plt.loglog(h, errors, "o-", label="Measured L2 error")
plt.loglog(h, reference, "--", label="Reference: order 2")
plt.xlabel("Grid spacing h")
plt.ylabel("L2 error")
plt.title("Serial Heat Solver - Convergence")
plt.grid(True, which="both", alpha=0.3)
plt.legend()
plt.tight_layout()
plt.savefig(results / "convergence.png", dpi=200)

print("N     Observed order")
for row in rows:
    p = row["p_obs"]
    print(f'{row["N"]:<5} {p:.4f}' if p != "" else f'{row["N"]:<5} -')

print("Saved results/convergence.csv")
print("Saved results/convergence.png")
