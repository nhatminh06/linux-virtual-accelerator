#!/usr/bin/env python3
import csv
import statistics
import sys
from pathlib import Path


def percentile(values, fraction):
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int((len(ordered) - 1) * fraction))]


if len(sys.argv) != 2:
    raise SystemExit(f"usage: {sys.argv[0]} FILE.csv")
path = Path(sys.argv[1])
groups = {}
windows = {}
with path.open(newline="") as source:
    for row in csv.DictReader(source):
        groups.setdefault(row["mode"], []).append(int(row["latency_ns"]))
        start, end = int(row["start_ns"]), int(row["end_ns"])
        old = windows.get(row["mode"], (start, end))
        windows[row["mode"]] = (min(old[0], start), max(old[1], end))
print("mode,count,mean_ns,p50_ns,p95_ns,p99_ns,commands_per_second")
for mode, values in sorted(groups.items()):
    duration = windows[mode][1] - windows[mode][0]
    throughput = len(values) * 1_000_000_000 / duration
    print(f"{mode},{len(values)},{statistics.fmean(values):.0f},"
          f"{percentile(values, .50)},{percentile(values, .95)},"
          f"{percentile(values, .99)},{throughput:.0f}")
