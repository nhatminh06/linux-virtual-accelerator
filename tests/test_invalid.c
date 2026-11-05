#include <stdint.h>
#include <signal.h>
#include <sys/ioctl.h>
#include "test_common.h"

static void interrupt_read(int signal)
{
	(void)signal;
}

int main(void)
{
	struct vaccel_submit request = { .operation = 99 };
	struct vaccel_completion completion;
	struct sigaction action = { .sa_handler = interrupt_read };
	char byte;
	int fd = open_vaccel(O_NONBLOCK);

	errno = 0;
	CHECK(ioctl(fd, _IO('X', 0), 0) == -1 && errno == ENOTTY);
	errno = 0;
	CHECK(ioctl(fd, VACCEL_IOC_ADD, (void *)(uintptr_t)1) == -1 && errno == EFAULT);
	errno = 0;
	CHECK(ioctl(fd, VACCEL_IOC_SUBMIT, &request) == -1 && errno == EINVAL);
	errno = 0;
	CHECK(read(fd, &byte, 1) == -1 && errno == EINVAL);
	close(fd);

	/* Closing with accepted work exercises worker-held client references. */
	fd = open_vaccel(0);
	request = (struct vaccel_submit) { .a = 1, .b = 2, .operation = VACCEL_OP_ADD };
	CHECK(ioctl(fd, VACCEL_IOC_SUBMIT, &request) == 0);
	close(fd);

	/* An unhandled restart would leave this test blocked instead of returning EINTR. */
	CHECK(sigemptyset(&action.sa_mask) == 0 && sigaction(SIGALRM, &action, NULL) == 0);
	fd = open_vaccel(0);
	alarm(1);
	errno = 0;
	CHECK(read(fd, &completion, sizeof(completion)) == -1 && errno == EINTR);
	alarm(0);
	close(fd);
	puts("test_invalid: PASS");
	return 0;
}
