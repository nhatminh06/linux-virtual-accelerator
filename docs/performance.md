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
