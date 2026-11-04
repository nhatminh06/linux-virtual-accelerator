#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
kdir="${KDIR:-}"
busybox="${BUSYBOX_INSTALL_DIR:-}"
output="${GUEST_BUILD_DIR:-${root}/build/guest}"
mkdir -p "${output}"
output="$(cd "${output}" && pwd)"
staging="$(mktemp -d "${output}/root.XXXXXX")"
trap 'rm -rf -- "${staging}"' EXIT

if [[ -z "${kdir}" || ! -f "${kdir}/arch/x86/boot/bzImage" ]]; then
	echo "Set KDIR to the configured, built guest kernel tree (with bzImage)." >&2
	exit 2
fi
if [[ -z "${busybox}" || ! -x "${busybox}/bin/busybox" ]]; then
	echo "Set BUSYBOX_INSTALL_DIR to a static BusyBox installation." >&2
	exit 2
fi

make -C "${root}/driver" KDIR="${kdir}"
make -C "${root}/userspace" clean all LDFLAGS=-static
make -C "${root}/tests" clean all LDFLAGS=-static
mkdir -p "${staging}"/{dev,proc,sys,tmp,root,vaccel}
cp -a "${busybox}/." "${staging}/"
cp "${root}/driver/vaccel.ko" "${root}/userspace/vaccel_client" \
	"${root}/userspace/vaccel_bench" "${root}"/tests/test_* "${root}/scripts/test-guest.sh" \
	"${staging}/vaccel/"
cat >"${staging}/init" <<'EOF'
#!/bin/sh
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
echo "vaccel guest ready"
exec setsid cttyhack /bin/sh
EOF
chmod +x "${staging}/init"
(cd "${staging}" && find . -print0 | cpio --null -o --format=newc 2>/dev/null) | gzip -9 >"${output}/initramfs.cpio.gz"
cp "${kdir}/arch/x86/boot/bzImage" "${output}/bzImage"
printf 'Guest artifacts: %s\n' "${output}"
