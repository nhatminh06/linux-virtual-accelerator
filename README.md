# Linux Virtual Accelerator

An educational, software-only Linux misc driver that models the control path of
an accelerator without pretending to be GPU hardware. `/dev/vaccel` supports a
synchronous add ioctl and bounded asynchronous submission with per-open
completions, blocking/nonblocking `read(2)`, and `poll(2)`.

```text
client fd A --ioctl--\                 /-- completion ring A -- read/poll A
client fd B --ioctl----> global FIFO -> worker
client fd C --ioctl--/                 \-- completion ring B/C (isolated)
```

## Build and use

Requirements are GNU make, a C compiler, and a configured kernel build tree.
The module must be built against the exact kernel that will run it.

```sh
./scripts/check-dependencies.sh
make build KDIR=/path/to/guest-kernel-build
# For a kernel built with Clang:
make build KDIR=/path/to/guest-kernel-build KBUILD_FLAGS=LLVM=1
```

Never load this experimental module on the host. In an isolated guest:

```sh
insmod driver/vaccel.ko
./userspace/vaccel_client sync 10 20
./userspace/vaccel_client async 10 20
make guest-test
rmmod vaccel
```

Both client commands print `30`. Overflow returns `ERANGE`; invalid asynchronous
operations return `EINVAL`; a full global queue or a client's 64-command
outstanding limit returns `EAGAIN`. Reads require a buffer whose size is a
positive multiple of the 32-byte completion record.

## QEMU workflow

Provide a bootable x86 kernel image and its exact build tree. The guest builder
packages a static PID 1, module, tests, and benchmarks; it never loads anything
into the host kernel. On x86-64-v4 hosts, use the project-local baseline musl
toolchain because QEMU TCG cannot execute the host's AVX-512 glibc startup code.

```sh
GUEST_CC="$(./scripts/build-musl.sh)"
KDIR=/path/to/linux-build KERNEL_IMAGE=/path/to/matching/vmlinuz \
  GUEST_CC="$GUEST_CC" ./scripts/build-guest.sh
QEMU_APPEND=ignore_loglevel ./scripts/run-qemu.sh > build/guest/qemu.log 2>&1
./scripts/validate-guest-log.sh build/guest/qemu.log
./scripts/extract-guest-results.py build/guest/qemu.log benchmarks/results/run
```

The static PID 1 runs every test, reloads the module 20 times, benchmarks four
queue depths and two concurrent clients, emits artifacts over serial, and powers
off. `scripts/run-qemu.sh` accepts
`GUEST_BUILD_DIR`, `KERNEL_IMAGE`, `INITRAMFS_IMAGE`, `QEMU_MEMORY`, `QEMU_SMP`,
`QEMU_CPU`, and `QEMU_APPEND`.

## Tests and benchmarks

- `make build`: module, userspace programs, and test compile checks.
- `make test`: runtime tests only when `/dev/vaccel` exists; otherwise BLOCKED.
- `make guest-test`: interactive-guest test helper; the automated init runs all five tests.
- `make bench`: write raw CSV and print p50/p95/p99 summaries.

The benchmark uses `CLOCK_MONOTONIC`, discards 100 warmups, and reports
end-to-end round-trip latency. Its synchronous boundary is ioctl entry through
return; asynchronous is submission immediately followed by completion read.
Run independent benchmark processes to measure multiple clients. QEMU numbers
measure this virtual control path, not physical GPU performance.

## Current validation

The independently audited QEMU run passed all 27 orchestration checks on Linux
7.2.11-1-cachyos, including five test binaries, 20 reload cycles, and 14
benchmark workloads. Raw evidence is under the gitignored
`benchmarks/results/audit-qemu-7.2.11-2026-11-07/` directory in this worktree.
See [testing](docs/testing.md), [performance](docs/performance.md), and
[limitations](docs/limitations.md).

## Design documentation

- [Architecture](docs/architecture.md)
- [Concurrency and ownership](docs/concurrency.md)
- [Testing](docs/testing.md)
- [Validation record](docs/validation.md)
- [Independent audit](docs/audit-summary.md)
- [Performance](docs/performance.md)
- [Limitations](docs/limitations.md)
- [Learning guide](docs/learning-guide.md)
