# Independent audit summary — 2026-11-07

Scope: driver/UAPI, five tests, guest PID 1, build/QEMU/result scripts,
ownership documentation, prior serial evidence, and benchmark artifacts at
commit `3c70ca8`.

## Findings

- **Critical:** none.
- **High — guest false PASS:** benchmark-phase module-load failure skipped the
  benchmarks without incrementing `failures`; missing files still emitted empty
  artifact markers. The harness now records that load, treats missing/write-failed
  artifacts as failures, and a host validator rejects incomplete logs.
- **Medium — 32-bit ioctl failure:** the fixed-width ABI had no compat handler on
  a `CONFIG_COMPAT=y` kernel. `compat_ptr_ioctl` now forwards the layout-compatible
  commands without changing the ABI. This is compile-verified and source-reasoned;
  no 32-bit guest toolchain was available for a runtime test.
- **Low — weak race coverage:** the close test could finish after its request.
  The delayed test now closes with 64 outstanding commands, verifies recovery
  after saturation, and four clients each exercise eight outstanding requests
  plus poll readiness.

## Verification status

- **Source-reviewed:** allocation/reference pairs, FIFO ownership, completion
  reservations, lock ordering, wait predicates, poll/read agreement, close,
  worker drain, init rollback, ioctl encoding, user copies, and overflow.
- **Runtime-tested:** native ABI, concurrent clients, interrupted/nonblocking
  reads, poll, saturation/recovery, pending close, 20 lifecycle cycles, shutdown,
  and benchmarks under Linux 7.2.11 QEMU TCG.
- **Reasoned only:** 32-bit compat execution and failures requiring forced kernel
  allocator/thread-creation injection.
- **Not tested:** KASAN, KCSAN, UBSAN, lockdep, debug-atomic-sleep, and debug-list;
  the installed guest kernel has these disabled.

An instrumented run would require a separate kernel source/build tree with
`CONFIG_KASAN`, `CONFIG_PROVE_LOCKING`, `CONFIG_DEBUG_ATOMIC_SLEEP`,
`CONFIG_DEBUG_LIST`, and `CONFIG_UBSAN`; run KCSAN in a separate configuration
for race detection. Expect roughly 10–25 GiB of build space, several GiB of RAM,
and tens of minutes depending on configuration and CPU. Build that kernel and
this module against the same output tree, then reuse `scripts/build-guest.sh`
and `scripts/run-qemu.sh`. This was deferred because no matching source tree is
local and the existing runtime run is clean; no large download was started.

No driver use-after-free, double-free, leaked accepted command, lost wakeup,
completion crossover, reverse lock order, sleep under spinlock, or queue-bound
violation was found. No public ABI layout or command number changed.

## Final update — 2026-11-09

The prior 32-bit runtime gap is closed. A static ELF32 i386 musl test ran on the
64-bit `CONFIG_COMPAT` guest and passed both ioctls, ABI layout checks, invalid
commands, and overflow behavior. Forced allocation/thread-creation failures and
KASAN/KCSAN/UBSAN/lockdep remain untested. The installed kernel did run the
suite with KFENCE and SLUB debugging enabled.
