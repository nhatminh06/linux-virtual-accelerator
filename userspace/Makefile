CC ?= cc
CPPFLAGS += -I../include
CFLAGS ?= -O2 -g
CFLAGS += -std=c11 -Wall -Wextra -Werror -D_GNU_SOURCE

.PHONY: all clean
all: vaccel_client vaccel_bench

guest-init: vaccel_guest_init.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -static $< -o $@

vaccel_client: vaccel_client.c ../include/vaccel_uapi.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< -o $@

vaccel_bench: vaccel_bench.c ../include/vaccel_uapi.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< -o $@

clean:
	$(RM) vaccel_client vaccel_bench guest-init
