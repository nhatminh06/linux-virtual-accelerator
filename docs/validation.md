# Validation record — 2026-11-04

Environment: x86-64 CachyOS host, GCC 16.2.1, Clang 23.1.3, GNU make 4.4.1,
QEMU 11.1.2, and GDB 18.1. No module was loaded into the host kernel.

| Check | Command | Result |
|---|---|---|
| Userspace warnings | `make -C userspace clean all` | PASS |
| Test compile warnings | `make -C tests clean all` | PASS |
| Static guest binaries | builds above with `LDFLAGS=-static` | PASS |
| Linux 7.2 module | `make -C driver KDIR=/usr/lib/modules/7.2.11-1-cachyos/build LLVM=1 clean all` | PASS |
| Linux 6.18 module | `make -C driver KDIR=/usr/lib/modules/6.18.55-2-cachyos-lts/build clean all` | PASS |
| Shell syntax | `bash -n scripts/*.sh` | PASS |
| Analysis syntax | `python3 -m py_compile benchmarks/analysis.py` | PASS |
| Analysis smoke test | `analysis.py` with two synthetic parser rows | PASS (parser only) |
| Runtime ABI tests | `make test` | BLOCKED: `/dev/vaccel` absent by design |
| QEMU integration | `scripts/build-guest.sh` then guest test | BLOCKED: no built guest `bzImage` or static BusyBox installation |
| Benchmarks | `make bench` in validated guest | NOT RUN |

The original `linux-kernel-lab` worktree remained clean after implementation.
