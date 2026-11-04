KDIR ?= /lib/modules/$(shell uname -r)/build
KBUILD_FLAGS ?= $(if $(shell grep -s '^CONFIG_CC_IS_CLANG=y' $(KDIR)/include/config/auto.conf),LLVM=1)

.PHONY: all build module userspace tests test guest-test bench clean check
all: build
build: module userspace tests

module:
	$(MAKE) -C driver KDIR=$(KDIR) $(KBUILD_FLAGS)

userspace:
	$(MAKE) -C userspace

tests:
	$(MAKE) -C tests

test: tests
	@echo "Runtime tests require /dev/vaccel; use 'make guest-test' in the QEMU guest."
	@test -e /dev/vaccel && $(MAKE) -C tests run || { echo "BLOCKED: /dev/vaccel is unavailable"; exit 2; }

guest-test:
	./scripts/test-guest.sh

bench: userspace
	@mkdir -p benchmarks/results
	./userspace/vaccel_bench 10000 > benchmarks/results/latest.csv
	./benchmarks/analysis.py benchmarks/results/latest.csv

check:
	./scripts/check-dependencies.sh

clean:
	$(MAKE) -C driver KDIR=$(KDIR) $(KBUILD_FLAGS) clean
	$(MAKE) -C userspace clean
	$(MAKE) -C tests clean
	$(RM) -r build
