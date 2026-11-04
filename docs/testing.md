# Testing

Host compile checks build userspace with `-Wall -Wextra -Werror` and build the
module through kbuild. Runtime testing is guest-only.

| Test | Coverage |
|---|---|
| `test_basic` | positive, negative, zero, signed overflow |
| `test_async` | nonblocking empty read, multiple IDs/results, poll readiness |
| `test_concurrent` | four processes, repeated isolated completions |
| `test_invalid` | unknown ioctl, bad pointer, bad operation, bad read size, close with work |
| `test_saturation` | reserved completion capacity and deterministic `EAGAIN` |

`scripts/test-guest.sh` loads the module, runs all tests, and unloads it even
after failure. For lifecycle stress, run it repeatedly from a guest shell. Check
`dmesg` after the run. With a custom guest kernel, enable KASAN, lockdep, and
debug-list options deliberately in that guest configuration; they are not
silently enabled by this project.

The guest runner reloads the module with the test-only `worker_delay_ms=20`
parameter for `test_saturation`; normal and benchmark runs use the zero default.
