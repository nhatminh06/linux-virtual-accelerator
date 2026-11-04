# Architecture

`vaccel.c` registers a misc device, which supplies the dynamic minor and creates
`/dev/vaccel` through devtmpfs. The UAPI uses fixed-width Linux types and has
12-byte compute, 24-byte submit, and 32-byte completion records.

`VACCEL_IOC_ADD` copies one compute record in, checks signed addition with
`check_add_overflow`, and copies the result out. `VACCEL_IOC_SUBMIT` validates
the operation and flags, allocates one bounded command, reserves a per-client
completion slot, assigns a module-lifetime ID, and appends it to the global
64-entry FIFO. Queue saturation is `EAGAIN`; commands are never dropped.

One kthread waits on `command_wait`, removes FIFO entries, performs the same safe
addition, and publishes each result to its owner's completion ring. `read`
copies complete records and frees reserved capacity. `poll` registers the
caller's wait queue and reports readable whenever that caller has completions.
The condition is checked after registration, so a wakeup cannot be missed.

Memory ownership is direct: the submitter allocates a command, the global queue
transfers it to the worker, and the worker frees it. A command holds a `kref` on
its client. The file holds the initial reference. Closing marks the client as
closing and drops the file reference; queued workers discard results for a
closed fd and release their references safely.

Initialization registers the device before starting the worker and rolls back
registration if thread creation fails. Module exit prevents new opens, then
`kthread_stop` wakes the worker; the worker drains accepted commands before it
exits.
