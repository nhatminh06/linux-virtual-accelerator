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
