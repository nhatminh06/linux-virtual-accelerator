#include <limits.h>
#include <sys/ioctl.h>
#include "test_common.h"

static void add(int fd, int a, int b, int expected)
{
	struct vaccel_compute request = { .a = a, .b = b };
	CHECK(ioctl(fd, VACCEL_IOC_ADD, &request) == 0);
	CHECK(request.result == expected);
}

int main(void)
{
	struct vaccel_compute request = { .a = INT_MAX, .b = 1 };
	int fd = open_vaccel(0);
	add(fd, 10, 20, 30);
	add(fd, -10, 3, -7);
	add(fd, 0, 0, 0);
	errno = 0;
	CHECK(ioctl(fd, VACCEL_IOC_ADD, &request) == -1 && errno == ERANGE);
	close(fd);
	puts("test_basic: PASS");
	return 0;
}
