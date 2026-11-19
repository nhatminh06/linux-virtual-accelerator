# Testing

Host compile checks build userspace with `-Wall -Wextra -Werror` and build the
module through kbuild. Runtime testing is guest-only.

| Test | Coverage |
|---|---|
| `test_basic` | positive, negative, zero, signed overflow |
| `test_async` | nonblocking empty read, multiple IDs/results, poll readiness |
| `test_concurrent` | four processes, eight outstanding requests each, poll and isolation |
| `test_invalid` | bad ioctl/pointer/operation/read size, interrupted blocking read, close with work |
| `test_saturation` | deterministic `EAGAIN`, recovery, close with 64 pending requests |
| `test_compat32` | 32-bit layouts, sync/async compat ioctls, invalid commands, and overflow on a 64-bit kernel |

The static guest PID 1 performs load/test/unload, delayed saturation testing, 20
additional load/unload cycles, benchmarks, serial artifact export, and poweroff.
The 2026-11-07 audit run produced 27 PASS markers and zero FAIL markers. KASAN, KCSAN,
UBSAN, lockdep, debug-atomic-sleep, and debug-list were not enabled in the
installed guest kernel; the serial log contained no kernel BUG, WARNING, Oops,
or panic during the successful run.

The guest runner reloads the module with the test-only `worker_delay_ms=20`
parameter for `test_saturation`; normal and benchmark runs use the zero default.
`scripts/validate-guest-log.sh` requires the exact 28-result marker set emitted
by the guest harness, including 12 benchmark markers, and rejects missing or
duplicate results, missing artifacts, guest failures, kernel diagnostics, and
incomplete shutdown. `tests/test_validate_guest_log.sh` checks those failure
cases against a copy of the qualified log.

The 2026-11-09 final run added a static i386 musl test. The 64-bit guest had
`CONFIG_COMPAT=y` and `CONFIG_IA32_EMULATION=y`; the binary was verified as
ELF32 before boot and passed both ioctl paths at runtime. All 28 orchestration
markers passed, including the prior five native tests and lifecycle coverage.
