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

Provide a built x86 guest kernel and a statically installed BusyBox. The guest
builder compiles the module against `KDIR`, packages the module and static test
binaries, and never touches the host kernel.

```sh
KDIR=/path/to/linux-build BUSYBOX_INSTALL_DIR=/path/to/busybox-install \
  ./scripts/build-guest.sh
./scripts/run-qemu.sh
# guest shell:
/vaccel/test-guest.sh
```

The source tree/commit, `.config`, compiler, and QEMU command are inputs owned by
the caller; record them alongside any results. `scripts/run-qemu.sh` accepts
`GUEST_BUILD_DIR`, `KERNEL_IMAGE`, `INITRAMFS_IMAGE`, `QEMU_MEMORY`, `QEMU_SMP`,
and `QEMU_APPEND`.

## Tests and benchmarks

- `make build`: module, userspace programs, and test compile checks.
- `make test`: runtime tests only when `/dev/vaccel` exists; otherwise BLOCKED.
- `make guest-test`: load, execute four integration tests, and unload in a guest.
- `make bench`: write raw CSV and print p50/p95/p99 summaries.

The benchmark uses `CLOCK_MONOTONIC`, discards 100 warmups, and reports
end-to-end round-trip latency. Its synchronous boundary is ioctl entry through
return; asynchronous is submission immediately followed by completion read.
Run independent benchmark processes to measure multiple clients. QEMU numbers
measure this virtual control path, not physical GPU performance.

## Current validation

Source and host compile results must be recorded from the current environment;
this repository does not ship fabricated benchmark or runtime results. Guest
integration is BLOCKED until a matching guest kernel and initramfs are booted.
See [testing](docs/testing.md), [performance](docs/performance.md), and
[limitations](docs/limitations.md).

## Design documentation

- [Architecture](docs/architecture.md)
- [Concurrency and ownership](docs/concurrency.md)
- [Testing](docs/testing.md)
- [Validation record](docs/validation.md)
- [Performance](docs/performance.md)
- [Limitations](docs/limitations.md)
- [Learning guide](docs/learning-guide.md)
