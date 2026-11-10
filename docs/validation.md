# Validation record — 2026-11-04

## Environment

- Host: x86-64, 12 logical CPUs, 15 GiB RAM, 178 GiB free workspace storage.
- Guest: Linux `7.2.11-1-cachyos`; installed `vmlinuz` and exact kbuild tree.
- Module vermagic: `7.2.11-1-cachyos SMP preempt mod_unload`.
- QEMU 11.1.2: TCG, i440FX, two Haswell vCPUs, 2 GiB RAM, serial console.
- Initramfs: static musl 1.2.5 PID 1, clients, tests, and benchmarks.
- Required config is present: modules/unload, initrd/gzip, devtmpfs, procfs,
  sysfs, ELF, printk, and 8250 serial console.

The host's static glibc is x86-64-v4 and crashed under TCG at AVX-512 startup.
The final guest uses baseline musl binaries. QEMU uses `panic=0` and
`-no-reboot`, so a failed init cannot look like repeated test attempts.

## Evidence

| Check | Command | Result | Evidence |
|---|---|---|---|
| Fresh build | `make clean && make build` | PASS | warnings-as-errors programs and kbuild module |
| Guest package | `GUEST_CC=build/musl/bin/musl-gcc KDIR=... scripts/build-guest.sh` | PASS | matching bzImage/module and static initramfs |
| QEMU boot | `QEMU_APPEND=ignore_loglevel scripts/run-qemu.sh` | PASS | guest uname marker for 7.2.11 x86-64 |
| Module/device | guest `finit_module`, `/dev/vaccel` check | PASS | both markers present |
| ABI tests | five test executables | PASS | 5/5 binaries, all assertions |
| Lifecycle | initial, delayed, 20-cycle, benchmark loads | PASS | 23 registrations and 23 unloads |
| Orchestration | all guest checks | PASS | 26 PASS, 0 FAIL, final `failures=0` |
| Kernel diagnostics | serial scan | PASS | no BUG, WARNING, Oops, panic, or sanitizer report |
| Benchmarks | 3 trials × 4 depths plus 2 clients | PASS | 14/14 CSV artifacts |
| Debug kernel | guest `.config` inspection | NOT RUN | KASAN/KCSAN/UBSAN/lockdep debug options disabled |
| Frozen lab | `git -C ../linux-kernel-lab status --porcelain` | PASS | no changes |

The expected out-of-tree and missing-signature taint messages appear once.
Signature enforcement is not active, and loading succeeds.

## Reproduction

```sh
cd /path/to/linux-virtual-accelerator
GUEST_CC="$(./scripts/build-musl.sh)"
COMPAT_CC="$(MUSL_ARCH=i386 ./scripts/build-musl.sh)"
KDIR=/usr/lib/modules/7.2.11-1-cachyos/build \
KERNEL_IMAGE=/usr/lib/modules/7.2.11-1-cachyos/vmlinuz \
GUEST_CC="$GUEST_CC" COMPAT_CC="$COMPAT_CC" ./scripts/build-guest.sh
timeout 180 env QEMU_APPEND=ignore_loglevel ./scripts/run-qemu.sh \
  > build/guest/qemu.log 2>&1
./scripts/validate-guest-log.sh build/guest/qemu.log
./scripts/extract-guest-results.py build/guest/qemu.log \
  benchmarks/results/qemu-7.2.11-2026-11-04
```

Run `benchmarks/analysis.py` on each extracted raw CSV. The VM powers itself
off; no host module or boot configuration is changed.

## Independent audit — 2026-11-07

The audit fixed a guest-harness false-pass path, enabled the standard compat
ioctl forwarder, and strengthened concurrency/lifetime tests. The rerun produced
27 PASS markers, zero FAIL markers, 23 registrations/unloads, 14 complete
artifacts, and `VACCEL_FINAL PASS failures=0`. The log validator also correctly
rejects the older log because it lacks explicit benchmark-module-load evidence.

Evidence: `benchmarks/results/audit-qemu-7.2.11-2026-11-07/`. The original
2026-11-04 benchmark directory was preserved unchanged.

## Final qualification — 2026-11-09

The installed guest configuration has `CONFIG_COMPAT=y` and
`CONFIG_IA32_EMULATION=y`. A static i386 musl test verified 12/24/32-byte UAPI
layouts, synchronous and asynchronous ioctls, invalid commands, and signed
overflow while running on the 64-bit guest. The complete rerun produced 28
PASS markers, zero FAIL markers, 23 registrations/unloads, 14 benchmark
artifacts, `VACCEL_FINAL PASS failures=0`, and a clean poweroff. The serial log
contained no BUG, WARNING, Oops, panic, or sanitizer report.

Evidence: `build/guest/qemu-release-2026-11-09.log` and
`benchmarks/results/release-qemu-7.2.11-2026-11-09/` (both gitignored).
