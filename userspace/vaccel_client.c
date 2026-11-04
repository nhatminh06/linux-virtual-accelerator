#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "vaccel_uapi.h"

static int parse_s32(const char *text, int32_t *value)
{
	char *end;
	long number;

	errno = 0;
	number = strtol(text, &end, 10);
	if (errno || *text == '\0' || *end != '\0' || number < INT32_MIN ||
	    number > INT32_MAX)
		return -1;
	*value = (int32_t)number;
	return 0;
}

static int run_sync(int fd, int32_t a, int32_t b)
{
	struct vaccel_compute request = { .a = a, .b = b };

	if (ioctl(fd, VACCEL_IOC_ADD, &request) == -1) {
		fprintf(stderr, "VACCEL_IOC_ADD: %s\n", strerror(errno));
		return 1;
	}
	printf("%d\n", request.result);
	return 0;
}

static int run_async(int fd, int32_t a, int32_t b)
{
	struct vaccel_submit request = {
		.a = a, .b = b, .operation = VACCEL_OP_ADD,
	};
	struct vaccel_completion completion;
	struct pollfd pfd = { .fd = fd, .events = POLLIN };
	ssize_t bytes;

	if (ioctl(fd, VACCEL_IOC_SUBMIT, &request) == -1) {
		fprintf(stderr, "VACCEL_IOC_SUBMIT: %s\n", strerror(errno));
		return 1;
	}
	if (poll(&pfd, 1, 5000) != 1) {
		fprintf(stderr, "poll: %s\n", errno ? strerror(errno) : "timeout");
		return 1;
	}
	bytes = read(fd, &completion, sizeof(completion));
	if (bytes != (ssize_t)sizeof(completion)) {
		fprintf(stderr, "read: %s\n", bytes < 0 ? strerror(errno) : "short read");
		return 1;
	}
	if (completion.request_id != request.request_id) {
		fprintf(stderr, "completion ID mismatch\n");
		return 1;
	}
	if (completion.status) {
		errno = -completion.status;
		fprintf(stderr, "async operation: %s\n", strerror(errno));
		return 1;
	}
	printf("%d\n", completion.result);
	return 0;
}

int main(int argc, char **argv)
{
	int32_t a, b;
	int fd, ret;

	_Static_assert(sizeof(struct vaccel_compute) == 12, "unexpected compute ABI");
	_Static_assert(sizeof(struct vaccel_submit) == 24, "unexpected submit ABI");
	_Static_assert(sizeof(struct vaccel_completion) == 32, "unexpected completion ABI");
	if (argc != 4 || (strcmp(argv[1], "sync") && strcmp(argv[1], "async")) ||
	    parse_s32(argv[2], &a) || parse_s32(argv[3], &b)) {
		fprintf(stderr, "usage: %s {sync|async} INT32 INT32\n", argv[0]);
		return 2;
	}
	fd = open("/dev/vaccel", O_RDWR | O_CLOEXEC);
	if (fd == -1) {
		fprintf(stderr, "open /dev/vaccel: %s\n", strerror(errno));
		return 1;
	}
	ret = !strcmp(argv[1], "sync") ? run_sync(fd, a, b) : run_async(fd, a, b);
	close(fd);
	return ret;
}
