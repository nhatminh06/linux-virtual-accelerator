#include <sys/ioctl.h>
#include <sys/wait.h>
#include "test_common.h"

static void client(int base)
{
	struct vaccel_submit request;
	struct vaccel_completion completion;
	int fd = open_vaccel(0), i;

	for (i = 0; i < 100; i++) {
		request = (struct vaccel_submit) {
			.a = base, .b = i, .operation = VACCEL_OP_ADD,
		};
		while (ioctl(fd, VACCEL_IOC_SUBMIT, &request))
			CHECK(errno == EAGAIN);
		CHECK(read(fd, &completion, sizeof(completion)) == sizeof(completion));
		CHECK(completion.request_id == request.request_id);
		CHECK(completion.status == 0 && completion.result == base + i);
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
