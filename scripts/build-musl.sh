#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
version=1.2.5
sha256=a9a118bbe84d8764da0ea0d28b3ab3fae8477fc7e4085d90102b8596fc7c75e4
archive="${root}/build/musl-${version}.tar.gz"
arch="${MUSL_ARCH:-native}"
configure_args=(--disable-shared)
make_args=()
cc="${MUSL_CC:-gcc}"
cflags="${MUSL_CFLAGS:-}"

case "${arch}" in
native)
	prefix="${root}/build/musl"
	;;
i386)
	prefix="${root}/build/musl-i386"
	configure_args+=(--target=i386)
	make_args+=(AR=ar RANLIB=ranlib)
	cflags="${cflags:--m32}"
	;;
*)
	echo "MUSL_ARCH must be native or i386" >&2
	exit 2
	;;
esac

if [[ -x "${prefix}/bin/musl-gcc" ]]; then
	printf '%s\n' "${prefix}/bin/musl-gcc"
	exit 0
fi
mkdir -p "${root}/build" "${prefix}"
if [[ ! -f "${archive}" ]]; then
	curl -fL "https://musl.libc.org/releases/musl-${version}.tar.gz" -o "${archive}"
fi
printf '%s  %s\n' "${sha256}" "${archive}" | sha256sum -c -
source_dir="$(mktemp -d "${root}/build/musl-src.XXXXXX")"
trap 'rm -rf -- "${source_dir}"' EXIT
tar -xzf "${archive}" --strip-components=1 -C "${source_dir}"
(
	cd "${source_dir}"
	CC="${cc}" CFLAGS="${cflags}" ./configure --prefix="${prefix}" "${configure_args[@]}"
	make -j "${MUSL_JOBS:-4}" "${make_args[@]}"
	make "${make_args[@]}" install
)
if [[ "${arch}" == i386 ]]; then
	sed -i '/^\*link:/{n;s/^/-m elf_i386 /;}' "${prefix}/lib/musl-gcc.specs"
fi
printf '%s\n' "${prefix}/bin/musl-gcc"
