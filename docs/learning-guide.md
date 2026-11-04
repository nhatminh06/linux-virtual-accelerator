# Learning guide

Study the implementation in this order after runtime validation.

1. **Character devices.** Read `vaccel_init`, `vaccel_exit`, and
   `vaccel_device`. A misc device removes manual major/class/device plumbing.
   Exercise: change the device name and observe devtmpfs. Interview prompt: what
   keeps module text resident while an fd is open?
2. **`file_operations`.** Follow `open`, `release`, `read`, `poll`, and
   `unlocked_ioctl` in `vaccel_fops`. The VFS supplies the per-open `struct file`.
   Exercise: trace one fd from open to release. Failure: missing `.owner` makes
   unload lifetime unsafe.
3. **ioctl.** Compare `_IOWR` declarations in `vaccel_uapi.h` with the command
   switch. Exercise: add a userspace call with an unknown command. Interview
   prompt: why return `ENOTTY`?
4. **User/kernel copying.** Find every `copy_from_user` and `copy_to_user`.
   These helpers validate access and may sleep; raw user pointers are never
   dereferenced. Failure: partial copy is reported by a nonzero return.
5. **Request structures.** Inspect fixed-width fields, ordering, size assertions,
   and reserved flags. Exercise: print sizes on 32- and 64-bit userspace.
6. **FIFO queues.** Trace head/tail/count for the global queue and per-client
   rings. Exercise: draw the indices across a wrap. Failure: confusing full and
   empty without a count.
7. **Mutexes and wait queues.** The spinlock protects non-sleeping global index
   changes; client mutexes cover copies and per-fd state; wait queues connect
   condition changes to sleepers. Interview prompt: why not hold a spinlock over
   `copy_to_user`?
8. **Kernel worker threads.** Trace `kthread_run`, the wait predicate, dequeue,
   and `kthread_stop`. Exercise: log worker PID in a disposable branch. Failure:
   stopping before draining leaks accepted commands.
9. **Per-client state.** `file->private_data` points to one client; a `kref`
   splits ownership between the fd and pending commands. Exercise: explain close
   with three queued commands. Interview prompt: why is a global completion FIFO
   incorrect?
10. **poll.** `poll_wait` registers the wait queue, then readiness is checked.
    Exercise: use `strace` on the async client. Failure: checking first can miss
    the transition between check and registration.
11. **Resource lifetime.** Follow allocation/free pairs for clients and commands,
    plus init rollback. Exercise: make an ownership table. Interview prompt:
    distinguish object lifetime from lock protection.
12. **Debugging.** Build with the exact guest kbuild tree, boot QEMU, inspect
    `dmesg`, and use a debug guest configuration with lockdep/KASAN. Never test by
    loading the module on the host.
13. **Benchmarking.** Read the monotonic timestamp boundaries in
    `vaccel_bench.c`, keep raw CSV, repeat trials, and separate submission from
    end-to-end latency. Exercise: launch 1, 2, and 4 processes and compare tails.
14. **mmap.** It is intentionally absent. A safe extension needs allocated pages,
    offset/size/permission validation, VMA reference ownership, and cleanup tests.
    Interview prompt: why can a VMA outlive the fd that created it?

## Source-file map

- `driver/vaccel.c`: device, queues, worker, and ownership. Key objects are
  `vaccel_client` and `vaccel_command`; the global queue owns commands until the
  worker frees them. Main failures are bad user memory, overflow, capacity, and
  interruption.
- `include/vaccel_uapi.h`: stable ABI shared by kernel and userspace. It owns no
  memory. Its principal failure mode is accidental layout or command-number
  change.
- `userspace/vaccel_client.c`: parsing, open/ioctl/poll/read/close control flow.
  The process owns all stack requests. Failures are printed from `errno`.
- `userspace/vaccel_bench.c`: warmup and raw timestamp capture. The output stream
  owns the measurement record; interruption can leave a valid partial CSV.
- `tests/*.c`: black-box ABI and concurrency checks. They assume an already
  loaded guest device and exit on the first violated invariant.
- `scripts/*.sh`: dependency reporting, guest packaging, QEMU boot, and guest
  lifecycle validation. Paths come from environment variables; generated guest
  artifacts live under `build/` by default.
- `benchmarks/analysis.py`: dependency-free percentile table from real CSV. It
  deliberately creates no invented data or synthetic chart.
