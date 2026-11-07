#include <poll.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include "test_common.h"

static void client(int base)
{
	struct vaccel_submit requests[8];
	struct vaccel_completion completion;
	struct pollfd pfd;
	int fd = open_vaccel(0), batch, i;

	for (batch = 0; batch < 96; batch += 8) {
		for (i = 0; i < 8; i++) {
			requests[i] = (struct vaccel_submit) {
				.a = base, .b = batch + i, .operation = VACCEL_OP_ADD,
			};
			while (ioctl(fd, VACCEL_IOC_SUBMIT, &requests[i]))
				CHECK(errno == EAGAIN);
		}
		pfd = (struct pollfd) { .fd = fd, .events = POLLIN };
		CHECK(poll(&pfd, 1, 5000) == 1 && (pfd.revents & POLLIN));
		for (i = 0; i < 8; i++) {
			CHECK(read(fd, &completion, sizeof(completion)) == sizeof(completion));
			CHECK(completion.request_id == requests[i].request_id);
			CHECK(completion.status == 0 && completion.result == base + batch + i);
		}
	}
	close(fd);
}

int main(void)
{
	pid_t children[4];
	int i, status;
	for (i = 0; i < 4; i++) {
		children[i] = fork();
		CHECK(children[i] >= 0);
		if (!children[i]) {
			client(i * 1000);
			_exit(0);
		}
	}
	for (i = 0; i < 4; i++) {
		CHECK(waitpid(children[i], &status, 0) == children[i]);
		CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	}
	puts("test_concurrent: PASS");
	return 0;
}
