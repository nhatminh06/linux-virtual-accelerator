#!/usr/bin/env bash
set -euo pipefail

log="${1:?usage: $0 QEMU_LOG}"
grep -q 'VACCEL_RESULT benchmark_module_load PASS' "${log}"
grep -q 'VACCEL_FINAL PASS failures=0' "${log}"
! grep -q 'VACCEL_RESULT .* FAIL' "${log}"
[[ "$(grep -c 'VACCEL_ARTIFACT_BEGIN' "${log}")" -eq 14 ]]
[[ "$(grep -c 'VACCEL_ARTIFACT_END' "${log}")" -eq 14 ]]
! grep -Eq 'BUG:|WARNING:|Oops:|Kernel panic|KASAN:|KCSAN:|UBSAN:|lockdep' "${log}"
grep -q 'reboot: Power down' "${log}"
echo "guest log validation: PASS"
