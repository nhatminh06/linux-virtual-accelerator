#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

static int failures;

static void result(const char *name, int passed)
{
	printf("VACCEL_RESULT %s %s\n", name, passed ? "PASS" : "FAIL");
	if (!passed)
		failures++;
}

static int module_load(const char *parameters)
{
	int fd = open("/vaccel/vaccel.ko", O_RDONLY | O_CLOEXEC);
	int ret;

	if (fd < 0)
		return -1;
	ret = syscall(SYS_finit_module, fd, parameters, 0);
	close(fd);
	return ret;
}

static int module_unload(void)
{
	return syscall(SYS_delete_module, "vaccel", O_NONBLOCK);
}

static int run(const char *program, char *const argv[], const char *output)
{
	pid_t child = fork();
	int status;

	if (child == 0) {
		if (output) {
			int fd = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (fd < 0 || dup2(fd, STDOUT_FILENO) < 0)
				_exit(126);
			close(fd);
		}
		execv(program, argv);
		_exit(127);
	}
	if (child < 0 || waitpid(child, &status, 0) != child)
		return -1;
	return WIFEXITED(status) ? WEXITSTATUS(status) : 128;
}

static pid_t start_benchmark(const char *output, const char *depth)
{
	pid_t child = fork();

	if (child == 0) {
		int fd = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		char *const argv[] = { "/vaccel/vaccel_bench", "1000", (char *)depth, NULL };
		if (fd < 0 || dup2(fd, STDOUT_FILENO) < 0)
			_exit(126);
		close(fd);
		execv(argv[0], argv);
		_exit(127);
	}
	return child;
}

static void emit_file(const char *path)
{
	char buffer[16384];
	ssize_t bytes;
	int fd = open(path, O_RDONLY);

	printf("VACCEL_ARTIFACT_BEGIN %s\n", path);
	while (fd >= 0 && (bytes = read(fd, buffer, sizeof(buffer))) > 0)
		if (write(STDOUT_FILENO, buffer, (size_t)bytes) != bytes)
			break;
	if (fd >= 0)
		close(fd);
	printf("VACCEL_ARTIFACT_END %s\n", path);
}

int main(void)
{
	static const char *tests[] = { "test_basic", "test_async", "test_concurrent", "test_invalid" };
	static const char *depths[] = { "1", "8", "32", "64" };
	struct utsname uts;
	char path[128], output[128];
	int i, trial, status;
	pid_t first, second;

	setvbuf(stdout, NULL, _IONBF, 0);
	mkdir("/proc", 0755); mkdir("/sys", 0755); mkdir("/dev", 0755); mkdir("/tmp", 01777);
	if (mount("proc", "/proc", "proc", 0, NULL) ||
	    mount("sysfs", "/sys", "sysfs", 0, NULL) ||
	    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL)) {
		perror("mount");
		return 1;
	}
	uname(&uts);
	printf("VACCEL_GUEST_UNAME %s %s %s\n", uts.sysname, uts.release, uts.machine);

	result("module_load", module_load("") == 0);
	result("device_node", access("/dev/vaccel", R_OK | W_OK) == 0);
	for (i = 0; i < (int)(sizeof(tests) / sizeof(tests[0])); i++) {
		snprintf(path, sizeof(path), "/vaccel/%s", tests[i]);
		char *const argv[] = { path, NULL };
		result(tests[i], run(path, argv, NULL) == 0);
	}
	result("module_unload", module_unload() == 0);

	result("delayed_module_load", module_load("worker_delay_ms=20") == 0);
	{
		char *const argv[] = { "/vaccel/test_saturation", NULL };
		result("test_saturation", run(argv[0], argv, NULL) == 0);
	}
	result("delayed_module_unload", module_unload() == 0);

	for (i = 0; i < 20; i++)
		if (module_load("") || module_unload())
			break;
	result("load_unload_20", i == 20);

	if (!failures && module_load("") == 0) {
		for (trial = 1; trial <= 3; trial++) {
			for (i = 0; i < (int)(sizeof(depths) / sizeof(depths[0])); i++) {
				snprintf(output, sizeof(output), "/tmp/depth%s-trial%d.csv", depths[i], trial);
				char *const argv[] = { "/vaccel/vaccel_bench", "1000", (char *)depths[i], NULL };
				result("benchmark", run(argv[0], argv, output) == 0);
			}
		}
		first = start_benchmark("/tmp/multiclient-1.csv", "32");
		second = start_benchmark("/tmp/multiclient-2.csv", "32");
		result("benchmark_multiclient_1", waitpid(first, &status, 0) == first && WIFEXITED(status) && !WEXITSTATUS(status));
		result("benchmark_multiclient_2", waitpid(second, &status, 0) == second && WIFEXITED(status) && !WEXITSTATUS(status));
		result("benchmark_module_unload", module_unload() == 0);
	}

	for (trial = 1; trial <= 3; trial++)
		for (i = 0; i < (int)(sizeof(depths) / sizeof(depths[0])); i++) {
			snprintf(output, sizeof(output), "/tmp/depth%s-trial%d.csv", depths[i], trial);
			emit_file(output);
		}
	emit_file("/tmp/multiclient-1.csv");
	emit_file("/tmp/multiclient-2.csv");
	printf("VACCEL_FINAL %s failures=%d\n", failures ? "FAIL" : "PASS", failures);
	sync();
	reboot(RB_POWER_OFF);
	return failures ? 1 : 0;
}
