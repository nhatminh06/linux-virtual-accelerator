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

The static guest PID 1 performs load/test/unload, delayed saturation testing, 20
additional load/unload cycles, benchmarks, serial artifact export, and poweroff.
The 2026-11-07 audit run produced 27 PASS markers and zero FAIL markers. KASAN, KCSAN,
UBSAN, lockdep, debug-atomic-sleep, and debug-list were not enabled in the
installed guest kernel; the serial log contained no kernel BUG, WARNING, Oops,
or panic during the successful run.

The guest runner reloads the module with the test-only `worker_delay_ms=20`
parameter for `test_saturation`; normal and benchmark runs use the zero default.
`scripts/validate-guest-log.sh` rejects missing benchmark-module-load evidence,
missing artifacts, guest failures, kernel diagnostics, and incomplete shutdown.
