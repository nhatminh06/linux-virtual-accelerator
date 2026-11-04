#!/usr/bin/env bash
set -euo pipefail

required=(bash make gcc qemu-system-x86_64 cpio gzip python3)
optional=(gdb sparse shellcheck)
missing=0

for command in "${required[@]}"; do
	if command -v "${command}" >/dev/null 2>&1; then
		printf '[ok] %s\n' "${command}"
	else
		printf '[missing] %s (required)\n' "${command}"
		missing=1
	fi
done
for command in "${optional[@]}"; do
	command -v "${command}" >/dev/null 2>&1 && printf '[ok] %s\n' "${command}" || printf '[--] %s (optional)\n' "${command}"
done
[[ -d "${KDIR:-/lib/modules/$(uname -r)/build}" ]] || {
	printf '[missing] kernel build tree: %s\n' "${KDIR:-/lib/modules/$(uname -r)/build}"
	missing=1
}
exit "${missing}"
