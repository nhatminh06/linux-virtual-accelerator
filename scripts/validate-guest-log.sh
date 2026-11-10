#!/usr/bin/env bash
set -euo pipefail

log="${1:?usage: $0 QEMU_LOG}"
grep -q 'VACCEL_RESULT test_compat32 PASS' "${log}"
grep -q 'VACCEL_RESULT benchmark_module_load PASS' "${log}"
grep -q 'VACCEL_FINAL PASS failures=0' "${log}"
if grep -q 'VACCEL_RESULT .* FAIL' "${log}"; then
	exit 1
fi
[[ "$(grep -c 'VACCEL_ARTIFACT_BEGIN' "${log}")" -eq 14 ]]
[[ "$(grep -c 'VACCEL_ARTIFACT_END' "${log}")" -eq 14 ]]
if grep -Eq 'BUG:|WARNING:|Oops:|Kernel panic|KASAN:|KCSAN:|UBSAN:|lockdep' "${log}"; then
	exit 1
fi
grep -q 'reboot: Power down' "${log}"
echo "guest log validation: PASS"
