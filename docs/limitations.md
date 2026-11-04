# Limitations

- Only signed 32-bit addition is implemented; this keeps attention on driver
  mechanics rather than fake accelerator math.
- Capacity is fixed at 64 globally and 64 outstanding commands per client.
- Request IDs are unique only within one module load and theoretically wrap.
- There is one worker, so execution is FIFO and intentionally serialized.
- No cancellation ioctl exists. Close safely discards eventual completions.
- The benchmark supports queue depths through 64; use parallel processes for
  multi-client contention measurements.
- No mmap interface is included. Proving page/VMA lifetime would add substantial
  complexity without improving the core queue and ownership lesson.
- Runtime behavior is unverified until the module is tested in a matching QEMU
  guest. Loading on the host is explicitly unsupported.
