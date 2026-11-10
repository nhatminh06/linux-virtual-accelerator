#!/usr/bin/env bash
set -eu

if ! dir="$(cd "$(dirname "$0")/.." 2>/dev/null && pwd)"; then
	dir="$(pwd)"
fi
if [ -d /vaccel ]; then dir=/vaccel; fi
module="${VACCEL_MODULE:-${dir}/driver/vaccel.ko}"
[ -f "${module}" ] || module="${dir}/vaccel.ko"

cleanup() { rmmod vaccel 2>/dev/null || true; }
trap cleanup EXIT
insmod "${module}"
[ -c /dev/vaccel ]
for test in test_basic test_async test_concurrent test_invalid; do
	"${dir}/${test}"
done
cleanup
insmod "${module}" worker_delay_ms=20
"${dir}/test_saturation"
cleanup
trap - EXIT
echo "guest integration: PASS"
