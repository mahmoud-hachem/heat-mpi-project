import csv
import statistics
from pathlib import Path
import numpy as np
import matplotlib.pyplot as plt

project_root = Path(__file__).resolve().parent.parent
folder = project_root / "results"
raw_folder = folder / "raw"
plot_folder = folder / "plots"
plot_folder.mkdir(parents=True, exist_ok=True)

groups = {
    "same_vm": [
        "same_heat1.csv",
        "same_heat2.csv",
        "same_heat3.csv"
    ],
    "cross_vm": [
        "heat2_heat1_direct.csv",
        "heat2_heat3_direct.csv",
        "heat1_heat3_direct.csv"
    ]
}

summary = []

for group, filenames in groups.items():
    plt.figure(figsize=(9, 5))

    for filename in filenames:
        with open(raw_folder / filename) as f:
            data = list(csv.DictReader(f))

        m = np.array([int(r["bytes"]) for r in data], dtype=float)
        t = np.array([float(r["oneway_ms"]) / 1000 for r in data])

        # Estimate startup latency from small messages
        ts = statistics.median(t[m <= 1024])

        # Fit transfer time using large messages
        mask = m >= 1048576
        tw = np.sum(m[mask] * (t[mask] - ts)) / np.sum(m[mask] ** 2)

        bandwidth = 1 / (tw * 1e6) if tw > 0 else float("nan")

        name = filename.replace(".csv", "")
        summary.append([name, ts * 1000, bandwidth])

        plt.loglog(m, t * 1000, "o", label=name)
        plt.loglog(m, (ts + tw * m) * 1000, "--", alpha=0.7)

    plt.xlabel("Message size (bytes)")
    plt.ylabel("One-way time (ms)")
    plt.title(group.replace("_", " ").title())
    plt.grid(True, which="both", alpha=0.3)
    plt.legend(fontsize=8)
    plt.tight_layout()
    plt.savefig(plot_folder / f"{group}_model.png", dpi=160)
    plt.close()

with open(folder / "network_summary.csv", "w", newline="") as f:
    writer = csv.writer(f)
    writer.writerow(["connection", "latency_ms", "bandwidth_MB_s"])
    writer.writerows(summary)

print("Connection             Latency (ms)    Bandwidth (MB/s)")
for name, latency, bandwidth in summary:
    print(f"{name:<22} {latency:>10.4f} {bandwidth:>15.3f}")

print(f"\nSaved {folder / 'network_summary.csv'} and graphs in {plot_folder}")
