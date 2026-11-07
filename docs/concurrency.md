# Concurrency and ownership

Shared command FIFO state is protected by `command_lock`, a spinlock held only
for index/count updates. It is never held during allocation, user copies,
computation, mutex acquisition, or sleep. Each open fd has a client mutex that
protects its completion ring, outstanding count, and closing flag.

Submission takes the client mutex and then briefly the command spinlock. The
worker releases the command spinlock before taking a client mutex. No path takes
these locks in the reverse nested order. Read and release take only the client
mutex. Poll registration itself is lockless; readiness inspection uses the
client mutex.

An accepted request increments `outstanding`, which counts pending commands plus
unread completions. Acceptance requires `outstanding < 64`; therefore each
accepted command owns one future completion slot. The worker cannot overflow a
client ring. Reading decrements the reservation. Closing causes each worker to
drop its result and reservation rather than publish to an unreachable ring.

The file's module reference normally prevents unload while operations are open.
After a close with pending work, client `kref`s prevent use-after-free. During
unload, the worker drains the FIFO before stopping. The ID counter is global and
monotonic for one module load; wraparound after 2^64 accepted submissions is an
explicit practical limitation.

## Verified invariants

1. The file owns one client reference; each accepted command owns one more.
2. The submit path transfers an allocated command to the FIFO only after taking
   its client reference and reserving one completion.
3. The worker is the only command destructor and drops exactly one client
   reference after publishing or cancelling the completion.
4. Closing marks the client first. Later worker completions follow the documented
   cancellation policy: decrement the reservation and publish nothing.
5. `outstanding` equals queued commands plus running commands plus unread
   completions. It never exceeds the 64-record completion ring.
6. The only nested order is client mutex then command spinlock. The worker drops
   the spinlock before taking a client mutex, so no reverse order exists.
7. Poll registers the wait queue before checking `count`; read checks the same
   `count` condition, so readiness and consumption agree without a lost wakeup.
