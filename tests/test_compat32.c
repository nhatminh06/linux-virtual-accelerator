#include <limits.h>
#include <stddef.h>
#include <poll.h>
#include <stdint.h>
#include <sys/ioctl.h>

#include "test_common.h"

int main(void)
{
	struct vaccel_compute compute = { .a = 10, .b = 20 };
	struct vaccel_submit submit = {
		.a = 10, .b = 20, .operation = VACCEL_OP_ADD,
	};
	struct vaccel_completion completion;
	struct pollfd pfd;
	int fd;

	_Static_assert(sizeof(void *) == 4, "compat test must be 32-bit");
	_Static_assert(sizeof(struct vaccel_compute) == 12, "compute ABI");
	_Static_assert(sizeof(struct vaccel_submit) == 24, "submit ABI");
	_Static_assert(sizeof(struct vaccel_completion) == 32, "completion ABI");
	_Static_assert(offsetof(struct vaccel_submit, operation) == 16, "submit layout");
	_Static_assert(offsetof(struct vaccel_completion, submitted_ns) == 16,
		       "completion layout");

	fd = open_vaccel(0);
	CHECK(ioctl(fd, VACCEL_IOC_ADD, &compute) == 0 && compute.result == 30);
	compute = (struct vaccel_compute) { .a = INT_MAX, .b = 1 };
	errno = 0;
	CHECK(ioctl(fd, VACCEL_IOC_ADD, &compute) == -1 && errno == ERANGE);
	errno = 0;
	CHECK(ioctl(fd, _IO('X', 0), 0) == -1 && errno == ENOTTY);

	CHECK(ioctl(fd, VACCEL_IOC_SUBMIT, &submit) == 0);
	pfd = (struct pollfd) { .fd = fd, .events = POLLIN };
	CHECK(poll(&pfd, 1, 5000) == 1 && (pfd.revents & POLLIN));
	CHECK(read(fd, &completion, sizeof(completion)) == sizeof(completion));
	CHECK(completion.request_id == submit.request_id);
	CHECK(completion.status == 0 && completion.result == 30);

	submit = (struct vaccel_submit) {
		.a = INT_MAX, .b = 1, .operation = VACCEL_OP_ADD,
	};
	CHECK(ioctl(fd, VACCEL_IOC_SUBMIT, &submit) == 0);
	CHECK(read(fd, &completion, sizeof(completion)) == sizeof(completion));
	CHECK(completion.request_id == submit.request_id);
	CHECK(completion.status == -ERANGE && completion.result == 0);

	submit = (struct vaccel_submit) { .operation = 99 };
	errno = 0;
	CHECK(ioctl(fd, VACCEL_IOC_SUBMIT, &submit) == -1 && errno == EINVAL);
	close(fd);
	puts("test_compat32: PASS");
	return 0;
}
