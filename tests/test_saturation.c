#include <sys/ioctl.h>
#include "test_common.h"

int main(void)
{
	struct vaccel_submit requests[VACCEL_QUEUE_CAPACITY + 1];
	struct vaccel_completion completion;
	unsigned int i;
	int fd = open_vaccel(0), closing_fd;

	for (i = 0; i < VACCEL_QUEUE_CAPACITY; i++) {
		requests[i] = (struct vaccel_submit) {
			.a = (int)i, .b = 1, .operation = VACCEL_OP_ADD,
		};
		CHECK(ioctl(fd, VACCEL_IOC_SUBMIT, &requests[i]) == 0);
	}
	requests[i] = (struct vaccel_submit) { .operation = VACCEL_OP_ADD };
	errno = 0;
	CHECK(ioctl(fd, VACCEL_IOC_SUBMIT, &requests[i]) == -1 && errno == EAGAIN);
	for (i = 0; i < VACCEL_QUEUE_CAPACITY; i++) {
		CHECK(read(fd, &completion, sizeof(completion)) == sizeof(completion));
		CHECK(completion.request_id == requests[i].request_id);
		CHECK(completion.status == 0 && completion.result == (int)i + 1);
	}
	requests[0] = (struct vaccel_submit) { .a = 40, .b = 2, .operation = VACCEL_OP_ADD };
	CHECK(ioctl(fd, VACCEL_IOC_SUBMIT, &requests[0]) == 0);
	CHECK(read(fd, &completion, sizeof(completion)) == sizeof(completion));
	CHECK(completion.request_id == requests[0].request_id && completion.result == 42);
	close(fd);

	closing_fd = open_vaccel(0);
	for (i = 0; i < VACCEL_QUEUE_CAPACITY; i++) {
		requests[i] = (struct vaccel_submit) {
			.a = (int)i, .b = 1, .operation = VACCEL_OP_ADD,
		};
		CHECK(ioctl(closing_fd, VACCEL_IOC_SUBMIT, &requests[i]) == 0);
	}
	close(closing_fd);
	puts("test_saturation: PASS");
	return 0;
}
