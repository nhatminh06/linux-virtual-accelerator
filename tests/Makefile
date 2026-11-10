CC ?= cc
CPPFLAGS += -I../include
CFLAGS ?= -O2 -g
CFLAGS += -std=c11 -Wall -Wextra -Werror -D_GNU_SOURCE
TESTS := test_basic test_async test_concurrent test_invalid test_saturation
COMPAT_TEST := test_compat32
CC32 ?= gcc

.PHONY: all compat32 run clean
all: $(TESTS)

compat32: $(COMPAT_TEST)

$(COMPAT_TEST): test_compat32.c test_common.h ../include/vaccel_uapi.h
	$(CC32) $(CPPFLAGS) $(CFLAGS) -m32 -static $(LDFLAGS) $< -o $@

%: %.c test_common.h ../include/vaccel_uapi.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< -o $@

run: all
	@for test in $(TESTS); do ./$$test; done

clean:
	$(RM) $(TESTS) $(COMPAT_TEST)
