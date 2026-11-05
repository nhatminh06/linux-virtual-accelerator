#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
kdir="${KDIR:-}"
output="${GUEST_BUILD_DIR:-${root}/build/guest}"
guest_cc="${GUEST_CC:-cc}"
mkdir -p "${output}"
output="$(cd "${output}" && pwd)"
staging="$(mktemp -d "${output}/root.XXXXXX")"
trap 'rm -rf -- "${staging}"' EXIT

kernel="${KERNEL_IMAGE:-${kdir%/build}/vmlinuz}"
if [[ -z "${kdir}" || ! -f "${kdir}/Makefile" ]]; then
	echo "Set KDIR to the configured guest kernel build tree." >&2
	exit 2
fi
if [[ ! -f "${kernel}" ]]; then
	echo "Set KERNEL_IMAGE to the bootable image matching KDIR." >&2
	exit 2
fi

make -C "${root}/driver" KDIR="${kdir}"
make -C "${root}/userspace" clean all guest-init CC="${guest_cc}" LDFLAGS=-static
make -C "${root}/tests" clean all CC="${guest_cc}" LDFLAGS=-static
mkdir -p "${staging}"/{dev,proc,sys,tmp,root,vaccel}
cp "${root}/driver/vaccel.ko" "${root}/userspace/vaccel_client" \
	"${root}/userspace/vaccel_bench" "${root}"/tests/test_* "${root}/scripts/test-guest.sh" \
	"${staging}/vaccel/"
cp "${root}/userspace/guest-init" "${staging}/init"
(cd "${staging}" && find . -print0 | cpio --null -o --format=newc 2>/dev/null) | gzip -9 >"${output}/initramfs.cpio.gz"
cp "${kernel}" "${output}/bzImage"
printf 'Guest artifacts: %s\n' "${output}"
