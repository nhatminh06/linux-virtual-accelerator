#include <linux/kref.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/overflow.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/uaccess.h>

#include "vaccel_uapi.h"

struct vaccel_client {
	struct kref ref;
	struct mutex lock;
	wait_queue_head_t completion_wait;
	struct vaccel_completion completions[VACCEL_QUEUE_CAPACITY];
	u32 head, tail, count, outstanding;
	bool closing;
};

struct vaccel_command {
	struct vaccel_client *client;
	struct vaccel_submit request;
	u64 submitted_ns;
};

static struct vaccel_command *command_queue[VACCEL_QUEUE_CAPACITY];
static u32 command_head, command_tail, command_count;
static DEFINE_SPINLOCK(command_lock);
static DECLARE_WAIT_QUEUE_HEAD(command_wait);
static atomic64_t next_request_id = ATOMIC64_INIT(0);
static struct task_struct *worker;
static unsigned int worker_delay_ms;
module_param(worker_delay_ms, uint, 0444);
MODULE_PARM_DESC(worker_delay_ms, "Test-only delay before each command (default 0)");

static void vaccel_client_free(struct kref *ref)
{
	struct vaccel_client *client = container_of(ref, struct vaccel_client, ref);

	kfree(client);
}

static int vaccel_add(s32 a, s32 b, s32 *result)
{
	return check_add_overflow(a, b, result) ? -ERANGE : 0;
}

static bool vaccel_work_ready(void)
{
	unsigned long flags;
	bool ready;

	spin_lock_irqsave(&command_lock, flags);
	ready = command_count != 0;
	spin_unlock_irqrestore(&command_lock, flags);
	return ready;
}

static struct vaccel_command *vaccel_dequeue(void)
{
	struct vaccel_command *command = NULL;
	unsigned long flags;

	spin_lock_irqsave(&command_lock, flags);
	if (command_count) {
		command = command_queue[command_head];
		command_head = (command_head + 1) % VACCEL_QUEUE_CAPACITY;
		command_count--;
	}
	spin_unlock_irqrestore(&command_lock, flags);
	return command;
}

static int vaccel_worker(void *unused)
{
	struct vaccel_completion completion;
	struct vaccel_command *command;
	struct vaccel_client *client;

	(void)unused;
	for (;;) {
		wait_event_interruptible(command_wait,
			kthread_should_stop() || vaccel_work_ready());
		command = vaccel_dequeue();
		if (!command) {
			if (kthread_should_stop())
				break;
			continue;
		}

		client = command->client;
		if (worker_delay_ms)
			msleep(worker_delay_ms);
		completion.request_id = command->request.request_id;
		completion.status = vaccel_add(command->request.a,
			command->request.b, &completion.result);
		if (completion.status)
			completion.result = 0;
		completion.submitted_ns = command->submitted_ns;
		completion.completed_ns = ktime_get_ns();

		mutex_lock(&client->lock);
		if (client->closing) {
			client->outstanding--;
		} else {
			/* outstanding <= capacity reserves this completion slot. */
			client->completions[client->tail] = completion;
			client->tail = (client->tail + 1) % VACCEL_QUEUE_CAPACITY;
			client->count++;
		}
		mutex_unlock(&client->lock);
		wake_up_interruptible(&client->completion_wait);
		kref_put(&client->ref, vaccel_client_free);
		kfree(command);
	}
	return 0;
}

static int vaccel_open(struct inode *inode, struct file *file)
{
	struct vaccel_client *client;

	(void)inode;
	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		return -ENOMEM;
	kref_init(&client->ref);
	mutex_init(&client->lock);
	init_waitqueue_head(&client->completion_wait);
	file->private_data = client;
	return 0;
}

static int vaccel_release(struct inode *inode, struct file *file)
{
	struct vaccel_client *client = file->private_data;

	(void)inode;
	mutex_lock(&client->lock);
	client->closing = true;
	mutex_unlock(&client->lock);
	wake_up_interruptible(&client->completion_wait);
	kref_put(&client->ref, vaccel_client_free);
	return 0;
}

static long vaccel_sync_add(unsigned long arg)
{
	struct vaccel_compute request;
	int ret;

	if (copy_from_user(&request, (void __user *)arg, sizeof(request)))
		return -EFAULT;
	ret = vaccel_add(request.a, request.b, &request.result);
	if (ret)
		return ret;
	if (copy_to_user((void __user *)arg, &request, sizeof(request)))
		return -EFAULT;
	return 0;
}

static long vaccel_submit(struct file *file, unsigned long arg)
{
	struct vaccel_client *client = file->private_data;
	struct vaccel_command *command;
	struct vaccel_submit request;
	unsigned long flags;
	long ret = 0;

	if (copy_from_user(&request, (void __user *)arg, sizeof(request)))
		return -EFAULT;
	if (request.operation != VACCEL_OP_ADD || request.flags)
		return -EINVAL;
	command = kmalloc(sizeof(*command), GFP_KERNEL);
	if (!command)
		return -ENOMEM;
	request.request_id = atomic64_inc_return(&next_request_id);
	if (copy_to_user((void __user *)arg, &request, sizeof(request))) {
		kfree(command);
		return -EFAULT;
	}

	mutex_lock(&client->lock);
	if (client->outstanding == VACCEL_QUEUE_CAPACITY) {
		ret = -EAGAIN;
		goto out_client;
	}
	command->client = client;
	command->request = request;
	command->submitted_ns = ktime_get_ns();

	spin_lock_irqsave(&command_lock, flags);
	if (command_count == VACCEL_QUEUE_CAPACITY) {
		spin_unlock_irqrestore(&command_lock, flags);
		ret = -EAGAIN;
		goto out_client;
	}
	kref_get(&client->ref);
	client->outstanding++;
	command_queue[command_tail] = command;
	command_tail = (command_tail + 1) % VACCEL_QUEUE_CAPACITY;
	command_count++;
	spin_unlock_irqrestore(&command_lock, flags);
	mutex_unlock(&client->lock);

	wake_up_interruptible(&command_wait);
	return 0;

out_client:
	mutex_unlock(&client->lock);
	kfree(command);
	return ret;
}

static long vaccel_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case VACCEL_IOC_ADD:
		return vaccel_sync_add(arg);
	case VACCEL_IOC_SUBMIT:
		return vaccel_submit(file, arg);
	default:
		return -ENOTTY;
	}
}

static ssize_t vaccel_read(struct file *file, char __user *buf, size_t count,
			   loff_t *offset)
{
	struct vaccel_client *client = file->private_data;
	struct vaccel_completion completion;
	size_t copied = 0;
	int ret;

	(void)offset;
	if (count < sizeof(completion) || count % sizeof(completion))
		return -EINVAL;

	for (;;) {
		mutex_lock(&client->lock);
		if (client->count)
			break;
		mutex_unlock(&client->lock);
		if (copied)
			return copied;
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		ret = wait_event_interruptible(client->completion_wait,
			READ_ONCE(client->count) != 0);
		if (ret)
			return ret;
	}

	while (client->count && copied + sizeof(completion) <= count) {
		completion = client->completions[client->head];
		if (copy_to_user(buf + copied, &completion, sizeof(completion))) {
			mutex_unlock(&client->lock);
			return copied ? (ssize_t)copied : -EFAULT;
		}
		client->head = (client->head + 1) % VACCEL_QUEUE_CAPACITY;
		client->count--;
		client->outstanding--;
		copied += sizeof(completion);
	}
	mutex_unlock(&client->lock);
	return copied;
}

static __poll_t vaccel_poll(struct file *file, poll_table *wait)
{
	struct vaccel_client *client = file->private_data;
	__poll_t mask = 0;

	poll_wait(file, &client->completion_wait, wait);
	mutex_lock(&client->lock);
	if (client->count)
		mask = EPOLLIN | EPOLLRDNORM;
	mutex_unlock(&client->lock);
	return mask;
}

static const struct file_operations vaccel_fops = {
	.owner = THIS_MODULE,
	.open = vaccel_open,
	.release = vaccel_release,
	.unlocked_ioctl = vaccel_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = compat_ptr_ioctl,
#endif
	.read = vaccel_read,
	.poll = vaccel_poll,
};

static struct miscdevice vaccel_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "vaccel",
	.fops = &vaccel_fops,
	.mode = 0660,
};

static int __init vaccel_init(void)
{
	int ret;

	worker = kthread_run(vaccel_worker, NULL, "vaccel-worker");
	if (IS_ERR(worker))
		return PTR_ERR(worker);
	ret = misc_register(&vaccel_device);
	if (ret) {
		kthread_stop(worker);
		return ret;
	}
	pr_info("vaccel: registered /dev/vaccel\n");
	return 0;
}

static void __exit vaccel_exit(void)
{
	misc_deregister(&vaccel_device);
	kthread_stop(worker); /* worker drains every accepted command first */
	pr_info("vaccel: unloaded\n");
}

module_init(vaccel_init);
module_exit(vaccel_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("linux-virtual-accelerator contributors");
MODULE_DESCRIPTION("Educational software-only virtual accelerator");
