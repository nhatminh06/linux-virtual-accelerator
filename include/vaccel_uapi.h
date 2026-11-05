#ifndef VACCEL_UAPI_H
#define VACCEL_UAPI_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#include <linux/types.h>
typedef __s32 vaccel_s32;
typedef __u32 vaccel_u32;
typedef __u64 vaccel_u64;
#else
#include <stdint.h>
#include <sys/ioctl.h>
typedef int32_t vaccel_s32;
typedef uint32_t vaccel_u32;
typedef uint64_t vaccel_u64;
#endif

#define VACCEL_QUEUE_CAPACITY 64U
#define VACCEL_OP_ADD 1U

struct vaccel_compute {
	vaccel_s32 a;
	vaccel_s32 b;
	vaccel_s32 result;
};

struct vaccel_submit {
	vaccel_u64 request_id; /* written by the kernel */
	vaccel_s32 a;
	vaccel_s32 b;
	vaccel_u32 operation;
	vaccel_u32 flags;
};

struct vaccel_completion {
	vaccel_u64 request_id;
	vaccel_s32 status; /* 0 or a negative errno */
	vaccel_s32 result;
	vaccel_u64 submitted_ns;
	vaccel_u64 completed_ns;
};

#define VACCEL_IOC_MAGIC 'V'
#define VACCEL_IOC_ADD _IOWR(VACCEL_IOC_MAGIC, 0, struct vaccel_compute)
#define VACCEL_IOC_SUBMIT _IOWR(VACCEL_IOC_MAGIC, 1, struct vaccel_submit)

#endif
