#include <poll.h>
#include <sys/ioctl.h>
#include "test_common.h"

int main(void)
{
	struct vaccel_submit requests[8];
	struct vaccel_completion completions[8];
	struct pollfd pfd;
	int fd = open_vaccel(O_NONBLOCK), i;

	errno = 0;
	CHECK(read(fd, completions, sizeof(*completions)) == -1 && errno == EAGAIN);
	for (i = 0; i < 8; i++) {
		requests[i] = (struct vaccel_submit) {
			.a = i, .b = i + 1, .operation = VACCEL_OP_ADD,
		};
		CHECK(ioctl(fd, VACCEL_IOC_SUBMIT, &requests[i]) == 0);
	}
	pfd = (struct pollfd) { .fd = fd, .events = POLLIN };
	CHECK(poll(&pfd, 1, 5000) == 1 && (pfd.revents & POLLIN));
	for (i = 0; i < 8; i++) {
		while (read(fd, &completions[i], sizeof(completions[i])) == -1)
			CHECK(errno == EAGAIN && poll(&pfd, 1, 5000) == 1);
		CHECK(completions[i].request_id == requests[i].request_id);
		CHECK(completions[i].status == 0 && completions[i].result == 2 * i + 1);
	}
	close(fd);
	puts("test_async: PASS");
	return 0;
}
