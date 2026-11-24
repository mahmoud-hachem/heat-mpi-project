import csv
import statistics
from pathlib import Path

groups = {}

for folder in sorted(Path("results").glob("serial_heat-*")):
    host = folder.name.split("_")[1]

    for log in folder.glob("*.log"):
        for line in log.read_text().splitlines():
            if line.startswith("decomp=serial "):
                data = dict(item.split("=", 1)
                            for item in line.split())

                N = int(data["N"])
                steps = int(data["steps"])
                time = float(data["t_total"])

                groups.setdefault(
                    (host, N, steps), []
                ).append(time)

rows = []

for (host, N, steps), times in sorted(groups.items()):

    median = statistics.median(times)
    updates = (N - 1)**2 * steps

    tc = median / updates
    rate = updates / median

    row = [
        host, N, steps, len(times),
        min(times), median, max(times),
        tc, tc * 1e9, rate / 1e6
    ]

    rows.append(row)

    print(f"{host:7} N={N:5} "
          f"runs={len(times)} "
          f"median={median:.9f} s "
          f"tc={tc*1e9:.4f} ns "
          f"rate={rate/1e6:.2f} Mupdates/s")

    if len(times) != 3:
        print("WARNING: Expected 3 repetitions")

with open("results/serial_summary.csv", "w",
          newline="") as f:

    writer = csv.writer(f)

    writer.writerow([
        "host", "N", "steps", "repetitions",
        "min_time", "median_time", "max_time",
        "tc_seconds", "tc_nanoseconds",
        "million_updates_per_second"
    ])

    writer.writerows(rows)

print("\nResults saved to results/serial_summary.csv")
print(f"Configurations analyzed: {len(rows)}/15")
print(f"Runs analyzed: {sum(len(t) for t in groups.values())}/45")
