CC ?= cc
CPPFLAGS += -I../include
CFLAGS ?= -O2 -g
CFLAGS += -std=c11 -Wall -Wextra -Werror -D_GNU_SOURCE
TESTS := test_basic test_async test_concurrent test_invalid test_saturation

.PHONY: all run clean
all: $(TESTS)

%: %.c test_common.h ../include/vaccel_uapi.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< -o $@

run: all
	@for test in $(TESTS); do ./$$test; done

clean:
	$(RM) $(TESTS)
