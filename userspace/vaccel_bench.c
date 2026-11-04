#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include "vaccel_uapi.h"

static uint64_t now_ns(void)
{
	struct timespec time;
	if (clock_gettime(CLOCK_MONOTONIC, &time)) {
		perror("clock_gettime");
		exit(1);
	}
	return (uint64_t)time.tv_sec * 1000000000ULL + time.tv_nsec;
}

int main(int argc, char **argv)
{
	struct vaccel_completion completion;
	struct vaccel_compute compute = { .a = 20, .b = 22 };
	struct vaccel_submit submit = { .a = 20, .b = 22, .operation = VACCEL_OP_ADD };
	struct vaccel_submit *submits;
	uint64_t *starts, start, end;
	char *tail;
	long iterations = 10000, depth = 1, i, j, batch;
	int fd;

	if (argc > 3) {
		fprintf(stderr, "usage: %s [iterations [queue-depth]]\n", argv[0]);
		return 2;
	}
	if (argc == 3) {
		errno = 0;
		depth = strtol(argv[2], &tail, 10);
		if (errno || *tail || depth < 1 || depth > VACCEL_QUEUE_CAPACITY) {
			fprintf(stderr, "queue-depth must be between 1 and %u\n",
				VACCEL_QUEUE_CAPACITY);
			return 2;
		}
	}
	submits = calloc((size_t)depth, sizeof(*submits));
	starts = calloc((size_t)depth, sizeof(*starts));
	if (!submits || !starts) {
		perror("calloc");
		return 1;
	}
	if (argc >= 2) {
		errno = 0;
		iterations = strtol(argv[1], &tail, 10);
		if (errno || *tail || iterations < 1) {
			fprintf(stderr, "iterations must be a positive integer\n");
			return 2;
		}
	}
	fd = open("/dev/vaccel", O_RDWR | O_CLOEXEC);
	if (fd == -1) {
		perror("open /dev/vaccel");
		return 1;
	}
	puts("mode,iteration,request_id,start_ns,end_ns,latency_ns");
	for (i = -100; i < iterations; i++) {
		start = now_ns();
		if (ioctl(fd, VACCEL_IOC_ADD, &compute)) {
			perror("VACCEL_IOC_ADD");
			return 1;
		}
		end = now_ns();
		if (i >= 0)
			printf("sync,%ld,0,%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n",
			       i, start, end, end - start);
	}
	for (i = 0; i < 100; i++) {
		start = now_ns();
		if (ioctl(fd, VACCEL_IOC_SUBMIT, &submit) ||
		    read(fd, &completion, sizeof(completion)) != sizeof(completion)) {
			perror("async request");
			return 1;
		}
	}
	for (i = 0; i < iterations; i += batch) {
		batch = iterations - i < depth ? iterations - i : depth;
		for (j = 0; j < batch; j++) {
			submits[j] = submit;
			starts[j] = now_ns();
			if (ioctl(fd, VACCEL_IOC_SUBMIT, &submits[j])) {
				perror("VACCEL_IOC_SUBMIT");
				return 1;
			}
		}
		for (j = 0; j < batch; j++) {
			if (read(fd, &completion, sizeof(completion)) != sizeof(completion)) {
				perror("read completion");
				return 1;
			}
			end = now_ns();
			if (completion.status || completion.request_id != submits[j].request_id) {
				errno = completion.status ? -completion.status : EPROTO;
				perror("async completion");
				return 1;
			}
			printf("async,%ld,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n",
			       i + j, (uint64_t)completion.request_id, starts[j], end,
			       end - starts[j]);
		}
	}
	close(fd);
	free(starts);
	free(submits);
	return 0;
}
