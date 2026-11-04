#ifndef VACCEL_UAPI_H
#define VACCEL_UAPI_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define VACCEL_QUEUE_CAPACITY 64U
#define VACCEL_OP_ADD 1U

struct vaccel_compute {
	__s32 a;
	__s32 b;
	__s32 result;
};

struct vaccel_submit {
	__u64 request_id; /* written by the kernel */
	__s32 a;
	__s32 b;
	__u32 operation;
	__u32 flags;
};

struct vaccel_completion {
	__u64 request_id;
	__s32 status; /* 0 or a negative errno */
	__s32 result;
	__u64 submitted_ns;
	__u64 completed_ns;
};

#define VACCEL_IOC_MAGIC 'V'
#define VACCEL_IOC_ADD _IOWR(VACCEL_IOC_MAGIC, 0, struct vaccel_compute)
#define VACCEL_IOC_SUBMIT _IOWR(VACCEL_IOC_MAGIC, 1, struct vaccel_submit)

#endif
