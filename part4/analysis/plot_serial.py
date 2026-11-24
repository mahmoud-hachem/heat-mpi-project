import csv
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from pathlib import Path

Path("plots").mkdir(exist_ok=True)

with open("results/serial_summary.csv") as f:
    rows = list(csv.DictReader(f))

hosts = {
    "heat-1": "Ali - heat-1",
    "heat-2": "heat-2",
    "heat-3": "Amer - heat-3"
}

graphs = [
    ("median_time", "Execution time (s)",
     "Serial Execution Time",
     "serial_execution_time.png"),

    ("tc_nanoseconds", "Time per update (ns)",
     "Computation Cost per Interior Point",
     "serial_computation_cost.png"),

    ("million_updates_per_second", "Million updates/s",
     "Serial Computation Throughput",
     "serial_throughput.png")
]

for column, ylabel, title, filename in graphs:

    plt.figure(figsize=(9, 6))

    for host, label in hosts.items():

        data = sorted(
            [r for r in rows if r["host"] == host],
            key=lambda r: int(r["N"])
        )

        x = [int(r["N"]) for r in data]
        y = [float(r[column]) for r in data]

        plt.plot(x, y, marker="o", label=label)

    plt.xscale("log", base=2)

    sizes = [512, 1024, 2048, 4096, 8192]
    plt.xticks(sizes, sizes)

    if column == "median_time":
        plt.yscale("log")

    plt.xlabel("Grid size N")
    plt.ylabel(ylabel)
    plt.title(title)
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()

    plt.savefig("plots/" + filename, dpi=200)
    plt.close()

    print("Generated:", filename)
