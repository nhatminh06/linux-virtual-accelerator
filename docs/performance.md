# Performance

`vaccel_bench` emits raw CSV timestamps from `CLOCK_MONOTONIC`. It performs 100
unreported warmups and then measures synchronous ioctl round trips and
asynchronous submit-to-read round trips. `analysis.py` uses only Python's
standard library to report count, throughput, mean, p50, p95, and p99 and writes
a latency-over-iteration SVG beside the CSV.

```sh
mkdir -p benchmarks/results
./userspace/vaccel_bench 10000 32 > benchmarks/results/run-1.csv
./benchmarks/analysis.py benchmarks/results/run-1.csv
```

Record `uname -a`, kernel commit/config, compiler version, QEMU version and full
command, host CPU/memory, vCPU count, iteration count, and parallel client count.
Repeat trials rather than treating one run as stable. Vary queue depth from 1 to
64 and run several benchmark processes concurrently for client scaling. The
baseline does not separately measure queue wait and execution time in userspace;
completion records expose kernel submission/completion timestamps for deeper
analysis.

QEMU results include virtualization, scheduling, and emulation overhead and are
not evidence about a physical accelerator or GPU.

## Measured QEMU results

Linux 7.2.11-1-cachyos ran under QEMU 11.1.2 TCG with two Haswell vCPUs and 2
GiB RAM. Each row is the median summary across three 1,000-command trials;
latencies are asynchronous end-to-end nanoseconds.

| Queue depth | p50 | p95 | p99 | commands/s |
|---:|---:|---:|---:|---:|
| 1 | 35,244 | 46,528 | 77,746 | 23,348 |
| 8 | 188,360 | 238,267 | 262,876 | 22,569 |
| 32 | 706,830 | 817,233 | 857,307 | 23,275 |
| 64 | 1,360,514 | 1,579,516 | 1,623,811 | 23,775 |

Two simultaneous depth-32 clients completed 1,000 asynchronous commands each.
Their p50/p95/p99 values were 653,366/757,093/790,287 ns and
837,314/1,249,024/1,338,564 ns. Per-process throughput was 24,580 and 18,469
commands/s. TCG scheduling explains part of the imbalance.

Raw CSV, summaries, SVGs, and the serial log are in the gitignored
`benchmarks/results/qemu-7.2.11-2026-11-04/` directory in the validated worktree.

The 2026-11-07 audit reran the identical 14 workloads after changes outside the
driver's command hot path. Median async throughput across queue depths was
12,999–19,167 commands/s, lower and noisier than the earlier 22,569–23,775
commands/s range. A single paired TCG run cannot distinguish host scheduling
noise from a regression, so no performance conclusion is claimed. Its raw data
is preserved separately under `benchmarks/results/audit-qemu-7.2.11-2026-11-07/`.
