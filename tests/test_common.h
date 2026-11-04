#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vaccel_uapi.h"

#define CHECK(condition) do { \
	if (!(condition)) { \
		fprintf(stderr, "%s:%d: CHECK failed: %s (%s)\n", \
			__FILE__, __LINE__, #condition, strerror(errno)); \
		exit(1); \
	} \
} while (0)

static inline int open_vaccel(int flags)
{
	int fd = open("/dev/vaccel", O_RDWR | O_CLOEXEC | flags);
	CHECK(fd >= 0);
	return fd;
}

#endif
