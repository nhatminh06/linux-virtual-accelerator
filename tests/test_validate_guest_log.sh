#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
validator="${root}/scripts/validate-guest-log.sh"
qualified_log="${1:-${root}/build/guest/qemu-release-2026-11-09.log}"
temporary="$(mktemp -d)"
trap 'rm -rf -- "${temporary}"' EXIT

expect_rejected() {
	local name="$1"

	if "${validator}" "${temporary}/${name}.log" >/dev/null 2>&1; then
		echo "validator accepted ${name}" >&2
		exit 1
	fi
}

"${validator}" "${qualified_log}" >/dev/null

sed '/VACCEL_RESULT test_basic PASS/d' "${qualified_log}" >"${temporary}/missing-required.log"
expect_rejected missing-required

cp "${qualified_log}" "${temporary}/duplicate-required.log"
printf '%s\n' 'VACCEL_RESULT test_basic PASS' >>"${temporary}/duplicate-required.log"
expect_rejected duplicate-required

sed '/VACCEL_RESULT test_compat32 PASS/d' "${qualified_log}" >"${temporary}/missing-compat.log"
expect_rejected missing-compat

sed '0,/VACCEL_RESULT benchmark PASS/{//d;}' "${qualified_log}" >"${temporary}/missing-benchmark.log"
expect_rejected missing-benchmark

cp "${qualified_log}" "${temporary}/explicit-failure.log"
printf '%s\n' 'VACCEL_RESULT injected_failure FAIL' >>"${temporary}/explicit-failure.log"
expect_rejected explicit-failure

sed '/VACCEL_FINAL PASS failures=0/d' "${qualified_log}" >"${temporary}/missing-final.log"
expect_rejected missing-final

sed '/reboot: Power down/d' "${qualified_log}" >"${temporary}/missing-shutdown.log"
expect_rejected missing-shutdown

echo "guest log validator tests: PASS"
