#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
guest="${GUEST_BUILD_DIR:-${root}/build/guest}"
exec qemu-system-x86_64 \
	-kernel "${KERNEL_IMAGE:-${guest}/bzImage}" \
	-initrd "${INITRAMFS_IMAGE:-${guest}/initramfs.cpio.gz}" \
	-append "console=ttyS0 panic=0 ${QEMU_APPEND:-}" \
	-m "${QEMU_MEMORY:-2G}" -smp "${QEMU_SMP:-2}" -cpu "${QEMU_CPU:-Haswell}" -no-reboot -nographic
