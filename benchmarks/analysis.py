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

width, height, margin = 900, 360, 40
maximum = max(max(values) for values in groups.values())
colors = ("#2563eb", "#dc2626", "#059669", "#7c3aed")
lines = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}">',
         '<rect width="100%" height="100%" fill="white"/>',
         f'<path d="M{margin} 10V{height-margin}H{width-10}" stroke="black" fill="none"/>']
for index, (mode, values) in enumerate(sorted(groups.items())):
    sampled = values[::max(1, len(values) // 1000)]
    points = " ".join(
        f"{margin + i * (width-margin-20) / max(1, len(sampled)-1):.1f},"
        f"{height-margin-value * (height-margin-20) / maximum:.1f}"
        for i, value in enumerate(sampled))
    color = colors[index % len(colors)]
    lines.append(f'<polyline points="{points}" fill="none" stroke="{color}" opacity=".8"/>')
    lines.append(f'<text x="{margin + 110 * index}" y="{height-8}" fill="{color}">{mode}</text>')
lines.append("</svg>")
plot = path.with_suffix(".svg")
plot.write_text("\n".join(lines), encoding="utf-8")
print(f"plot: {plot}", file=sys.stderr)
