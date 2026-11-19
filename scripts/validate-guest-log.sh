#!/usr/bin/env bash
set -euo pipefail

log="${1:?usage: $0 QEMU_LOG}"

require_count() {
	local marker="$1" expected="$2" actual

	actual="$(sed 's/\r$//' "${log}" | grep -Fxc "${marker}" || true)"
	if [[ "${actual}" -ne "${expected}" ]]; then
		printf 'expected %s occurrence(s) of %q, found %s\n' \
			"${expected}" "${marker}" "${actual}" >&2
		exit 1
	fi
}

while read -r name count; do
	require_count "VACCEL_RESULT ${name} PASS" "${count}"
done <<'EOF'
module_load 1
device_node 1
test_basic 1
test_async 1
test_concurrent 1
test_invalid 1
test_compat32 1
module_unload 1
delayed_module_load 1
test_saturation 1
delayed_module_unload 1
load_unload_20 1
benchmark_module_load 1
benchmark 12
benchmark_multiclient_1 1
benchmark_multiclient_2 1
benchmark_module_unload 1
EOF

require_count 'VACCEL_FINAL PASS failures=0' 1
pass_count="$(sed 's/\r$//' "${log}" | grep -c '^VACCEL_RESULT .* PASS$' || true)"
if [[ "${pass_count}" -ne 28 ]]; then
	printf 'expected 28 PASS results, found %s\n' "${pass_count}" >&2
	exit 1
fi

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
