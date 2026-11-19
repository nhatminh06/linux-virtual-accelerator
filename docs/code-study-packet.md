# Linux Virtual Accelerator: Code Study Packet

Repository baseline: `622036824f91529a4cc73016a30e6ed9470aa41e`

This packet is derived from the source at that commit. Statements labeled
**Observed** describe code or recorded results. Statements labeled **Inferred**
explain consequences of that code. The inference is not presented as a separate
runtime measurement.

## Eight-session study plan

| Session | Focus | Read first | Outcome |
|---:|---|---|---|
| 1 | UAPI and ABI | `include/vaccel_uapi.h` | Explain ioctl encoding, fixed-width layouts, and error conventions. |
| 2 | Device and file lifecycle | `vaccel_init`, `vaccel_open`, `vaccel_release`, `vaccel_exit` | Trace module, device, file, and client-state lifetimes. |
| 3 | Synchronous execution | `vaccel_client.c:run_sync`, `vaccel_sync_add`, `vaccel_add` | Trace one ioctl across the user/kernel boundary. |
| 4 | Asynchronous pipeline | `vaccel_submit`, `vaccel_worker` | Explain the FIFO, worker, completion publication, and request IDs. |
| 5 | Ownership and concurrency | client/command structures, locks, krefs | Prove why close and unload do not free live state. |
| 6 | Readiness and errors | `vaccel_read`, `vaccel_poll`, invalid/async tests | Explain blocking, nonblocking, poll, wakeups, and interruption. |
| 7 | Validation system | six tests and guest PID 1 | Map claims to tests and distinguish source review from runtime evidence. |
| 8 | Performance and interviews | `vaccel_bench.c`, `analysis.py`, questions below | Explain measured latency/throughput and practice systems interviews. |

---

## 1. Exact repository file inventory

This is the tracked source/document inventory at the validated baseline, plus
this study packet. Generated `build/` files and gitignored benchmark results are
not source files.

### Repository root

| File | Purpose |
|---|---|
| `.gitignore` | Excludes kbuild products, compiled programs, guest builds, and generated benchmark results. |
| `Makefile` | Top-level entry points for module, userspace, tests, guest tests, benchmarks, dependency checks, and cleanup. |
| `README.md` | Project overview, usage, QEMU workflow, and current validation summary. |

### Driver and shared ABI

| File | Purpose |
|---|---|
| `driver/Makefile` | External-module kbuild wrapper; detects a Clang-built kernel and supplies `LLVM=1`. |
| `driver/vaccel.c` | Misc device, synchronous ioctl, asynchronous FIFO, worker, per-client completions, read/poll, and lifetime management. |
| `include/vaccel_uapi.h` | Public kernel/userspace ABI: types, operations, structures, capacities, and ioctl numbers. |

### Userspace

| File | Purpose |
|---|---|
| `userspace/Makefile` | Builds the client, benchmark, and static guest PID 1. |
| `userspace/vaccel_client.c` | Command-line client implementing `sync` and `async` operations. |
| `userspace/vaccel_bench.c` | Generates synchronous and asynchronous raw latency CSV at selectable queue depth. |
| `userspace/vaccel_guest_init.c` | Static PID 1 that mounts guest filesystems, loads/unloads the module, runs tests and benchmarks, emits artifacts, and powers off. |

### Tests

| File | Purpose |
|---|---|
| `tests/Makefile` | Builds five native tests plus the 32-bit compat test. |
| `tests/test_common.h` | Shared `CHECK` assertion and `/dev/vaccel` open helper. |
| `tests/test_basic.c` | Synchronous results and signed-overflow behavior. |
| `tests/test_async.c` | Nonblocking empty reads, submissions, poll readiness, IDs, and results. |
| `tests/test_concurrent.c` | Four clients with multiple outstanding requests, poll, ordering, and isolation. |
| `tests/test_invalid.c` | Unsupported ioctl, bad pointer/operation/read size, close with work, and interrupted blocking read. |
| `tests/test_saturation.c` | Per-client capacity, `EAGAIN`, recovery, and closing with 64 outstanding commands. |
| `tests/test_compat32.c` | Static i386 ABI layouts, compat ioctls, invalid commands, and overflow. |

### Guest, build, and evidence scripts

| File | Purpose |
|---|---|
| `scripts/build-guest.sh` | Builds matching module/static programs and packages a minimal initramfs with the selected kernel image. |
| `scripts/build-musl.sh` | Downloads a pinned, checksum-verified musl release and builds a baseline static compiler wrapper. |
| `scripts/check-dependencies.sh` | Reports required and optional host tools and checks for a kbuild tree. |
| `scripts/run-qemu.sh` | Boots the kernel/initramfs under headless QEMU TCG with bounded default resources. |
| `scripts/test-guest.sh` | Manual interactive-guest load/test/unload helper. |
| `scripts/extract-guest-results.py` | Extracts serial-delimited benchmark CSV artifacts from a QEMU log. |
| `scripts/validate-guest-log.sh` | Rejects failed/incomplete guest runs, missing artifacts, kernel diagnostics, and incomplete poweroff. |

### Benchmark analysis

| File | Purpose |
|---|---|
| `benchmarks/analysis.py` | Computes count, mean, p50/p95/p99, throughput, and an SVG latency plot from raw CSV. |

### Documentation

| File | Purpose |
|---|---|
| `docs/architecture.md` | Concise architecture and data-flow description. |
| `docs/concurrency.md` | Locks, lock ordering, reference ownership, close policy, and verified invariants. |
| `docs/testing.md` | Coverage map and guest validation behavior. |
| `docs/validation.md` | Exact environment, commands, PASS evidence, and audit update. |
| `docs/performance.md` | Measurement boundaries, results, and QEMU limitations. |
| `docs/limitations.md` | Deliberate feature and validation limits. |
| `docs/learning-guide.md` | Topic-by-topic review guide and exercises. |
| `docs/audit-summary.md` | Independent correctness/reliability audit findings and verification status. |
| `docs/code-study-packet.md` | This self-contained source and interview study packet. |

---

## 2. Public UAPI

The authoritative definitions are `include/vaccel_uapi.h:4-45`:

```c
#ifdef __KERNEL__
#include <linux/ioctl.h>
#include <linux/types.h>
typedef __s32 vaccel_s32;
typedef __u32 vaccel_u32;
typedef __u64 vaccel_u64;
#else
#include <stdint.h>
#include <sys/ioctl.h>
typedef int32_t vaccel_s32;
typedef uint32_t vaccel_u32;
typedef uint64_t vaccel_u64;
#endif

#define VACCEL_QUEUE_CAPACITY 64U
#define VACCEL_OP_ADD 1U

struct vaccel_compute {
    vaccel_s32 a;
    vaccel_s32 b;
    vaccel_s32 result;
};

struct vaccel_submit {
    vaccel_u64 request_id;
    vaccel_s32 a;
    vaccel_s32 b;
    vaccel_u32 operation;
    vaccel_u32 flags;
};

struct vaccel_completion {
    vaccel_u64 request_id;
    vaccel_s32 status;
    vaccel_s32 result;
    vaccel_u64 submitted_ns;
    vaccel_u64 completed_ns;
};

#define VACCEL_IOC_MAGIC 'V'
#define VACCEL_IOC_ADD \
    _IOWR(VACCEL_IOC_MAGIC, 0, struct vaccel_compute)
#define VACCEL_IOC_SUBMIT \
    _IOWR(VACCEL_IOC_MAGIC, 1, struct vaccel_submit)
```

### Why the types are conditional

**Observed:** kernel compilation uses Linux `__s32/__u32/__u64`; userspace uses
the C fixed-width types. Both sides therefore agree on bit widths. The client
has compile-time assertions that the three structures are 12, 24, and 32 bytes
(`userspace/vaccel_client.c:80-82`).

### Constants

- `VACCEL_QUEUE_CAPACITY = 64`: global FIFO capacity and each client's maximum
  outstanding requests/completion storage.
- `VACCEL_OP_ADD = 1`: the only asynchronous operation.
- `VACCEL_IOC_MAGIC = 'V'`: ioctl type namespace used in both command numbers.

### `struct vaccel_compute` — 12 bytes

| Field | Direction | Meaning |
|---|---|---|
| `a` | userspace → kernel | First signed 32-bit operand. |
| `b` | userspace → kernel | Second signed 32-bit operand. |
| `result` | kernel → userspace | Signed sum on success. |

`VACCEL_IOC_ADD` uses `_IOWR`: the same pointed-to structure is read and then
written. Success returns `0`. Failures observed in the implementation are
`EFAULT` for an invalid user pointer and `ERANGE` for signed overflow.

### `struct vaccel_submit` — 24 bytes

| Field | Direction | Meaning |
|---|---|---|
| `request_id` | kernel → userspace | Module-lifetime monotonic identifier assigned with `atomic64_inc_return`. |
| `a` | userspace → kernel | First signed 32-bit operand. |
| `b` | userspace → kernel | Second signed 32-bit operand. |
| `operation` | userspace → kernel | Must equal `VACCEL_OP_ADD`. |
| `flags` | userspace → kernel | Reserved; must currently be zero. |

`VACCEL_IOC_SUBMIT` also uses `_IOWR`. Success means the command was accepted,
owns a client reference, and has a reserved completion slot. Failures are:

- `EFAULT`: input or output structure could not be copied.
- `EINVAL`: unsupported operation or nonzero flags.
- `ENOMEM`: command allocation failed.
- `EAGAIN`: global FIFO or this client's outstanding limit is full.

**Observed subtlety:** the kernel writes `request_id` before taking the queue
locks (`vaccel.c:181-185`). An ioctl that later returns `EAGAIN` may therefore
leave an ID in the user's structure, but the nonzero return means that ID was
not accepted and will not complete. IDs can have gaps.

### `struct vaccel_completion` — 32 bytes

| Field | Meaning |
|---|---|
| `request_id` | Matches the accepted submission. |
| `status` | `0` for success or a negative kernel errno, currently `-ERANGE` for asynchronous addition overflow. |
| `result` | Sum on success; explicitly zeroed on overflow. |
| `submitted_ns` | `ktime_get_ns()` timestamp recorded before FIFO insertion. |
| `completed_ns` | `ktime_get_ns()` timestamp after computation, before publication. |

Completions are obtained with `read`, not another ioctl. `read` returns a byte
count that is a multiple of 32.

### Unsupported and compat commands

- Any ioctl number other than the two exact constants returns `ENOTTY`
  (`vaccel.c:219-228`).
- When the kernel has `CONFIG_COMPAT`, `compat_ptr_ioctl` forwards 32-bit user
  pointers to the same native handler (`vaccel.c:286-296`).
- **Runtime-validated after the original packet:** a static i386 client exercised
  both ioctls on the 64-bit `CONFIG_COMPAT` guest on 2026-11-09.

---

## 3. Driver function map

Line references are for baseline commit `6220368`.

| Lines | Symbol | Role |
|---:|---|---|
| 15-22 | `struct vaccel_client` | Per-open state: reference, mutex, wait queue, fixed completion ring, indices/counts, and close flag. |
| 24-28 | `struct vaccel_command` | One allocated asynchronous command; points to its owning client. |
| 30-38 | global state | Fixed request FIFO, indices, spinlock, worker wait queue, ID counter, worker pointer, and test delay parameter. |
| 40-45 | `vaccel_client_free` | Final `kref` release callback; frees per-open state. |
| 47-50 | `vaccel_add` | Shared signed-add operation using `check_add_overflow`; returns `-ERANGE` on overflow. |
| 52-61 | `vaccel_work_ready` | Reads global FIFO readiness under the spinlock for the worker wait predicate. |
| 63-76 | `vaccel_dequeue` | Removes at most one command under the spinlock and transfers ownership to the worker. |
| 78-121 | `vaccel_worker` | Waits, dequeues, optionally delays, computes, publishes/cancels completion, wakes readers, and releases command/client ownership. |
| 123-136 | `vaccel_open` | Allocates and initializes one `vaccel_client`, then stores it in `file->private_data`. |
| 138-149 | `vaccel_release` | Marks the client closing, wakes waiters, and drops the file's client reference. |
| 151-164 | `vaccel_sync_add` | Copies a compute request in, adds safely, and copies the result out. |
| 166-217 | `vaccel_submit` | Validates, allocates, assigns ID, reserves client capacity, enqueues globally, and wakes the worker. |
| 219-229 | `vaccel_ioctl` | Exact ioctl dispatcher; unsupported commands return `-ENOTTY`. |
| 231-271 | `vaccel_read` | Validates record-sized reads, blocks or returns `EAGAIN`, copies completions, and releases reservations. |
| 273-284 | `vaccel_poll` | Registers the client wait queue and reports readable when `client->count > 0`. |
| 286-296 | `vaccel_fops` | Connects VFS operations to this driver, including the compat ioctl forwarder. |
| 298-303 | `vaccel_device` | Misc-device descriptor for `/dev/vaccel`, dynamic minor, mode `0660`. |
| 305-319 | `vaccel_init` | Starts the worker, registers the misc device, and rolls back the worker on registration failure. |
| 321-326 | `vaccel_exit` | Deregisters the device, drains/stops the worker, and logs unload. |

---

## 4. Full synchronous trace: `./vaccel_client sync 10 20`

### Userspace path

1. `main` verifies four arguments and selects the `sync` mode
   (`vaccel_client.c:75-87`).
2. `parse_s32` parses `"10"` and `"20"` with `strtol`, rejecting trailing text,
   conversion errors, and values outside signed 32-bit range (`:14-26`).
3. `open("/dev/vaccel", O_RDWR | O_CLOEXEC)` enters the driver's `.open`
   (`:88-92`).

### Open path in the kernel

4. `vaccel_open` allocates zero-filled `struct vaccel_client` memory with
   `kzalloc` (`vaccel.c:123-130`).
5. It initializes the client's reference count to one, mutex, and completion
   wait queue, then stores the pointer in `file->private_data` (`:131-135`).

### Ioctl path

6. `run_sync` builds `{ a = 10, b = 20, result = 0 }` and calls
   `ioctl(fd, VACCEL_IOC_ADD, &request)` (`vaccel_client.c:28-35`).
7. VFS calls `vaccel_ioctl`, whose exact command switch selects
   `vaccel_sync_add` (`vaccel.c:219-224`).
8. `copy_from_user` copies all 12 bytes into a kernel stack structure. A bad
   pointer would return `-EFAULT` (`:151-157`).
9. `vaccel_add(10, 20, &result)` calls `check_add_overflow`. It writes `30` and
   returns `0` (`:47-50`, `:158-160`). Overflow would return `-ERANGE` without a
   result copy back.
10. `copy_to_user` writes the structure containing `result = 30` back. Failure
    returns `-EFAULT`; otherwise the ioctl returns `0` (`:161-163`).
11. `run_sync` prints `30` (`vaccel_client.c:36`).

### Close path

12. `main` calls `close(fd)` (`:94`). VFS eventually invokes `vaccel_release`.
13. Release takes the client mutex, sets `closing = true`, unlocks, wakes any
    waiters, and drops the file's one `kref` (`vaccel.c:138-147`).
14. No asynchronous commands hold references in this trace, so the count reaches
    zero and `vaccel_client_free` calls `kfree` (`:40-45`).

**Observed:** the synchronous path never uses the global FIFO or worker.

---

## 5. Full asynchronous trace: `./vaccel_client async 10 20`

### Userspace submission

1. Argument parsing and open are the same as the synchronous trace.
2. `run_async` creates a zero-initialized request with `a = 10`, `b = 20`,
   `operation = VACCEL_OP_ADD`, and `flags = 0` (`vaccel_client.c:40-47`).
3. It calls `VACCEL_IOC_SUBMIT` (`:49-52`).

### Kernel acceptance

4. `vaccel_ioctl` dispatches to `vaccel_submit` (`vaccel.c:219-225`).
5. `copy_from_user` imports 24 bytes. The kernel rejects any operation other than
   ADD or nonzero flags (`:174-177`).
6. `kmalloc` creates one `vaccel_command` (`:178-180`). Until enqueue succeeds,
   this local submit path owns that allocation.
7. `atomic64_inc_return` assigns the request ID, and `copy_to_user` returns it to
   the client (`:181-185`).
8. The client mutex is acquired. If this fd already has 64 outstanding requests,
   submission fails with `-EAGAIN` and frees the command (`:187-190`, `:213-216`).
9. The command stores the client pointer, copied request, and submission timestamp
   (`:192-194`).
10. Under `command_lock`, the driver checks the global FIFO's 64-command bound.
    If full, it unlocks and fails cleanly (`:196-200`).
11. On acceptance it performs `kref_get(client)`, increments `outstanding`, stores
    the command at `command_tail`, advances the tail modulo 64, and increments
    `command_count` (`:202-207`). The FIFO now owns the command.
12. The locks are released and `wake_up_interruptible(command_wait)` wakes the
    worker (`:208-211`). The ioctl returns `0`.

### Poll and worker execution

13. Userspace calls `poll` with `POLLIN` and a five-second timeout
    (`vaccel_client.c:53-56`).
14. `vaccel_poll` first registers `client->completion_wait` with the poll table,
    then checks `client->count` under the client mutex (`vaccel.c:273-283`).
15. Independently, `vaccel_worker` wakes when either work exists or shutdown is
    requested (`:78-88`).
16. `vaccel_dequeue` removes the head command under `command_lock`; ownership
    moves from the FIFO to the worker (`:63-75`).
17. The worker performs `vaccel_add(10, 20)`, producing status `0` and result
    `30`, and records completion time (`:95-104`).
18. Under the owning client's mutex, the worker writes the completion at the
    client's tail, advances it modulo 64, and increments `count` (`:106-114`).
19. It unlocks, wakes `completion_wait`, drops the command's client reference,
    and frees the command (`:115-118`).
20. Poll observes `EPOLLIN | EPOLLRDNORM`, which userspace receives as `POLLIN`.

The worker may finish before userspace enters poll. That is still correct:
`vaccel_poll` checks `count` after registering, so it returns readable immediately.

### Completion read and close

21. Userspace reads exactly one 32-byte completion (`vaccel_client.c:57-61`).
22. `vaccel_read` validates the buffer size, sees `count > 0`, and copies the
    completion while holding the client mutex (`vaccel.c:231-263`).
23. Only after a successful copy does it advance `head`, decrement `count`, and
    decrement `outstanding`, releasing the reserved completion slot (`:264-270`).
24. Userspace verifies the ID, verifies `status == 0`, and prints `30`
    (`vaccel_client.c:62-72`).
25. `close` follows the same release path as the synchronous trace. The worker's
    reference has already been dropped and the completion consumed, so the file
    reference is the final reference.

---

## 6. Queue and completion architecture

```text
                 shared by every open file

 ioctl SUBMIT     +------------------------------------------+
 client A ------> | global command_queue[64]                 |
 client B ------> | head -> [cmd A1][cmd B1][cmd A2] -> tail |
 client C ------> | command_lock protects head/tail/count    |
                  +----------------------+-------------------+
                                         |
                                         | dequeue one
                                         v
                              +------------------------+
                              | vaccel-worker kthread  |
                              | safe signed addition   |
                              +-----+------------+-----+
                                    |            |
                  command.client ---+            +--- request/result/status
                                    |
             +----------------------+----------------------+
             |                      |                      |
             v                      v                      v
 +----------------------+ +----------------------+ +----------------------+
 | client A state       | | client B state       | | client C state       |
 | completions[64]      | | completions[64]      | | completions[64]      |
 | head/tail/count      | | head/tail/count      | | head/tail/count      |
 | outstanding          | | outstanding          | | outstanding          |
 | mutex + wait queue   | | mutex + wait queue   | | mutex + wait queue   |
 +----------+-----------+ +----------+-----------+ +----------+-----------+
            | read/poll              | read/poll              | read/poll
            v                        v                        v
       process A                  process B                  process C
```

The global FIFO decides execution order. The client pointer inside each command
decides completion ownership. Therefore one client cannot read another client's
completion.

---

## 7. Ownership and synchronization

### Allocations and references

#### `vaccel_client`

- Allocated once per successful `open` with `kzalloc` (`vaccel.c:128`).
- `kref_init` creates the file's initial ownership reference (`:131`).
- Every accepted asynchronous command takes one more reference with `kref_get`
  (`:202`).
- File release calls `kref_put(..., vaccel_client_free)` at `:147`.
- The worker calls the same `kref_put` for each command reference at `:117`.
- The final drop invokes `vaccel_client_free`, which frees the client (`:40-45`).

#### `vaccel_command`

- Allocated in `vaccel_submit` at `:178`.
- Before acceptance, submit owns it and frees it on every error path.
- After insertion into `command_queue`, the FIFO owns it.
- Dequeue transfers ownership to the one worker.
- The worker always frees it at `:118` after publication or close cancellation.

### What each lock protects

| Synchronization | Protected state | May sleep while held? |
|---|---|---|
| `command_lock` spinlock | Global `command_queue`, `command_head`, `command_tail`, `command_count` | No. Its critical sections only inspect/move pointers and indices. |
| `client->lock` mutex | Per-client completion entries and indices, `count`, `outstanding`, `closing` | Yes. `copy_to_user` occurs while held. |
| `atomic64_t next_request_id` | Cross-client request-ID allocation | Not a lock; atomic increment. |
| `command_wait` | Worker sleeping until work or stop | Predicate reads queue state under `command_lock`. |
| `completion_wait` | Read/poll notification for one client | Predicate is that client's `count != 0`. |

### Lock ordering

Submission takes `client->lock` and then briefly `command_lock`. The worker
dequeues and releases `command_lock` before taking `client->lock`. No path holds
the global spinlock while acquiring a client mutex, copying user memory,
computing, sleeping, or freeing memory.

### Close and cancellation

`vaccel_release` sets `closing = true` before dropping the file reference. A
worker that later finishes a command takes the same mutex, observes `closing`,
decrements `outstanding`, and does not publish an unreachable completion
(`vaccel.c:106-109`). It still drops the command's client reference and frees the
command. This is the explicit cancellation policy for close.

The test with `worker_delay_ms=20` submits 64 requests and closes immediately
(`tests/test_saturation.c:31-38`), ensuring command-held references outlive the
file reference during guest validation.

### Module unload

`.owner = THIS_MODULE` prevents normal module removal while open file references
exist. Exit first deregisters the misc device, preventing new opens, then calls
`kthread_stop` (`vaccel.c:321-325`). The worker's loop dequeues available commands
even when stop is requested; it exits only when stop is requested and dequeue
finds no command (`:85-93`). Thus accepted queued work is drained before module
text can disappear.

### Why accepted completions cannot be silently lost

The invariant is:

```text
client.outstanding = queued + running + unread completions
```

Submission requires `outstanding < 64`, then increments it while holding the
client mutex. Each accepted command therefore reserves one of the client's 64
completion positions before the worker can publish. The reservation remains
until either:

1. `read` successfully copies the completion and decrements `outstanding`, or
2. close cancellation decrements it without publishing.

The global queue independently rejects its 65th queued command. No acceptance
path overwrites a completion or drops a command without following one of those
two policies.

---

## 8. `read`, `poll`, wakeups, and interruption

### `read`

1. The requested size must be at least 32 and exactly divisible by 32; otherwise
   `-EINVAL` (`vaccel.c:239-241`).
2. With an empty completion queue and `O_NONBLOCK`, it returns `-EAGAIN`
   (`:243-251`).
3. With an empty blocking fd, it sleeps in `wait_event_interruptible` until
   `READ_ONCE(client->count) != 0` (`:252-255`).
4. Once data exists, it copies as many whole records as fit. A record is removed
   only after its copy succeeds (`:258-267`).
5. If a later copy fails after earlier records were copied, read returns the
   positive partial byte count. If the first copy fails, it returns `-EFAULT`.

### `poll`

`poll_wait` registers `completion_wait` before the mutex-protected readiness
check (`vaccel.c:278-282`). This ordering covers both races:

- Completion already exists: the check returns readable immediately.
- Completion arrives after registration: the worker's wakeup notifies poll.

Poll readiness and read use the same condition—`client->count > 0`—so a readable
result corresponds to at least one complete 32-byte record, subject to another
reader consuming it first if threads share one fd.

### Wakeups

- Successful submission wakes `command_wait` (`vaccel.c:210`).
- Worker completion wakes only the owning client's `completion_wait` (`:116`).
- Release also wakes that wait queue (`:146`), although VFS file lifetime normally
  prevents release from running while an operation still owns the file object.

### Interrupted reads

Because `wait_event_interruptible` is signal-aware, a pending signal causes it to
return a negative restart code. The syscall layer exposes this to userspace as
`read == -1` and `errno == EINTR` when the signal action does not request restart.
`test_invalid.c:35-42` installs a `SIGALRM` handler without `SA_RESTART`, blocks
in read, and verifies `EINTR`.

---

## 9. Test-to-behavior map

| Executable | Actual checks |
|---|---|
| `test_basic` | `10+20`, negative input, zero, and `INT_MAX+1 -> ERANGE` through synchronous ioctl. |
| `test_async` | Empty nonblocking read returns `EAGAIN`; eight accepted commands; poll readiness; every completion ID, status, and result; completion isolation within one fd. |
| `test_concurrent` | Four processes/open files; each repeatedly holds eight outstanding commands; poll under contention; FIFO order per client; correct IDs/results; no cross-client completions. |
| `test_invalid` | Unknown ioctl → `ENOTTY`; bad pointer → `EFAULT`; bad async operation → `EINVAL`; one-byte read → `EINVAL`; close after accepted work; signal-interrupted blocking read → `EINTR`. |
| `test_saturation` | Exactly 64 outstanding commands accepted; 65th returns `EAGAIN`; all 64 complete correctly; submit/read works again after draining; separate client closes with 64 delayed outstanding commands. |
| `test_compat32` | ELF32 execution on the x86-64 guest; exact UAPI sizes/offsets; sync/async compat ioctls; unknown command; synchronous and asynchronous overflow. |

The static guest PID 1 adds integration coverage: module/device creation,
ordinary and delayed module load/unload, 20 repeated lifecycle cycles, benchmark
module lifecycle, 14 benchmark artifacts, serial evidence, and poweroff.

**Observed result:** the 2026-11-07 audited run produced 27 PASS markers, zero
FAIL markers, 23 registrations/unloads, 14 artifacts, and clean poweroff.
The 2026-11-09 final run added `test_compat32` and produced 28 PASS markers
with the same lifecycle and artifact counts.

---

## 10. Benchmark methodology and queue-depth latency

### Measurement boundaries

`vaccel_bench` uses `CLOCK_MONOTONIC` (`vaccel_bench.c:14-22`).

- Synchronous start: immediately before `VACCEL_IOC_ADD`.
- Synchronous end: immediately after ioctl returns (`:68-77`).
- Asynchronous start: immediately before each `VACCEL_IOC_SUBMIT` (`:89-95`).
- Asynchronous end: immediately after that command's completion read returns
  (`:97-110`).

It performs 100 unreported synchronous warmups and 100 unreported asynchronous
submit/read warmups. Measured rows contain start, end, and `end-start` nanoseconds.

For queue depth `D`, it submits up to `D` commands before reading that batch in
submission order. The guest runs three 1,000-iteration trials at depths 1, 8,
32, and 64, then two depth-32 benchmark processes concurrently. The analyzer
computes mean, p50, p95, p99, time-window throughput, and an SVG.

### Why deeper queues had higher latency

**Observed:** the validated 2026-11-04 medians were:

| Depth | Async p50 | Async p95 | Async p99 | commands/s |
|---:|---:|---:|---:|---:|
| 1 | 35,244 ns | 46,528 ns | 77,746 ns | 23,348 |
| 8 | 188,360 ns | 238,267 ns | 262,876 ns | 22,569 |
| 32 | 706,830 ns | 817,233 ns | 857,307 ns | 23,275 |
| 64 | 1,360,514 ns | 1,579,516 ns | 1,623,811 ns | 23,775 |

**Inferred from the implementation:**

1. There is one worker, so execution is serialized even when submission is
   batched.
2. End-to-end latency includes FIFO waiting, worker scheduling, computation,
   completion publication, and userspace read delay.
3. The benchmark submits the whole batch before reading. Early completions wait
   in the client ring until submission finishes; later commands wait behind
   earlier commands in the FIFO.
4. Throughput stayed roughly flat while latency rose. Deeper queues kept the
   single worker fed but did not add execution capacity.
5. These are QEMU TCG control-path results, not GPU or physical accelerator
   performance. The 2026-11-07 rerun was noisier, demonstrating host-scheduling
   sensitivity rather than a statistically established code regression.

---

## 11. Twenty NVIDIA systems software interview questions

1. Why is a misc device appropriate here, and what work does it replace compared
   with manual character-device registration?
2. What does `.owner = THIS_MODULE` protect, and why are client `kref`s still
   necessary?
3. Trace ownership of a command from `kmalloc` through FIFO insertion, dequeue,
   worker completion, and `kfree`.
4. Give a concrete close-with-pending-work sequence and prove why it cannot cause
   a client use-after-free.
5. State the `outstanding` invariant and show why a published completion cannot
   overflow the per-client ring.
6. Why does the global FIFO use a spinlock while each client uses a mutex?
7. Which operations would be illegal while holding `command_lock`, and why?
8. What is the lock ordering? Show why the worker does not create an ABBA
   deadlock with submission.
9. Explain how `wait_event_interruptible` and `wake_up_interruptible` avoid a
   busy-waiting worker.
10. Why does `poll_wait` happen before the readiness check? Describe the lost
    wakeup that the opposite ordering could allow.
11. How do blocking read, `O_NONBLOCK` read, and an interrupted read differ in
    return value and errno?
12. Why must user pointers be accessed with `copy_from_user`/`copy_to_user`
    rather than direct dereference?
13. What does `_IOWR` encode, and what ABI break would occur if a structure's
    size changed?
14. Why is `check_add_overflow` preferable to computing the signed sum and then
    checking it?
15. Why does asynchronous overflow appear in `completion.status` rather than as
    the submit ioctl's errno?
16. What does `compat_ptr_ioctl` do here, and why is runtime 32-bit validation
    still a separate requirement?
17. During module unload, why does this worker drain queued commands instead of
    exiting immediately when `kthread_should_stop()` becomes true?
18. If this design used multiple workers, which ordering and completion-capacity
    assumptions would need to be reconsidered?
19. Why does queue depth increase end-to-end latency without materially raising
    throughput in these measurements?
20. Which bugs would KASAN, KCSAN, lockdep, and debug-atomic-sleep each be most
    likely to find in a driver like this?

---

## 12. Important limitations

### Observed implementation limits

- Only signed 32-bit addition exists; this is a control-path simulator, not a GPU
  compute implementation.
- One worker serializes all operations.
- Global queued capacity is 64; each client may have 64 total outstanding
  commands plus unread completions.
- There is no cancellation ioctl. Closing the fd is the cancellation boundary,
  and results from its unfinished commands are discarded safely.
- Request IDs are unique only within one module load and theoretically wrap.
- The interface has no DMA, interrupts from hardware, memory pinning, mmap,
  command buffers, GPU scheduling, or device reset/recovery.
- `worker_delay_ms` is test instrumentation, not simulated hardware timing.

### Validation limits

- The 32-bit compat ioctl forwarder is runtime-tested with a static i386 client
  on the x86-64 guest; other compat architectures remain untested.
- The installed guest kernel did not enable KASAN, KCSAN, UBSAN, lockdep,
  debug-atomic-sleep, debug-list, or kmemleak.
- Allocation failure and forced `kthread_run`/`misc_register` failure paths were
  source-reviewed, not exercised with fault injection.
- QEMU TCG timing is noisy and not representative of NVIDIA hardware.
- Absence of a warning in the tested run is evidence for that workload, not a
  proof that every possible interleaving is defect-free.

### Review boundary

The packet explains the implementation as it exists. It does not propose mmap,
DMA, GPU APIs, multiple workers, or other extensions, because those would change
the learning target rather than explain it.
