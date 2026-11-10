#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
kdir="${KDIR:-}"
output="${GUEST_BUILD_DIR:-${root}/build/guest}"
guest_cc="${GUEST_CC:-cc}"
compat_cc="${COMPAT_CC:-}"
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
if [[ -z "${compat_cc}" ]]; then
	echo "Set COMPAT_CC to a static i386 compiler (MUSL_ARCH=i386 scripts/build-musl.sh)." >&2
	exit 2
fi

make -C "${root}/driver" KDIR="${kdir}"
make -C "${root}/userspace" clean all guest-init CC="${guest_cc}" LDFLAGS=-static
make -C "${root}/tests" clean all CC="${guest_cc}" LDFLAGS=-static
make -C "${root}/tests" compat32 CC32="${compat_cc}"
mkdir -p "${staging}"/{dev,proc,sys,tmp,root,vaccel}
cp "${root}/driver/vaccel.ko" "${root}/userspace/vaccel_client" \
	"${root}/userspace/vaccel_bench" "${root}"/tests/test_basic \
	"${root}"/tests/test_async "${root}"/tests/test_concurrent \
	"${root}"/tests/test_invalid "${root}"/tests/test_saturation \
	"${root}"/tests/test_compat32 "${root}/scripts/test-guest.sh" \
	"${staging}/vaccel/"
cp "${root}/userspace/guest-init" "${staging}/init"
(cd "${staging}" && find . -print0 | cpio --null -o --format=newc 2>/dev/null) | gzip -9 >"${output}/initramfs.cpio.gz"
cp "${kernel}" "${output}/bzImage"
printf 'Guest artifacts: %s\n' "${output}"
