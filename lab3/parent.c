#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <sys/wait.h>

#define SHM_SIZE 4096

static void make_name(char *out, const char *prefix, int pid)
{
	size_t p = 0;
	while (prefix[p]) { out[p] = prefix[p]; ++p; }
	char tmp[12]; int i = 0;
	if (pid == 0) tmp[i++] = '0';
	while (pid > 0) { tmp[i++] = (char)('0' + pid % 10); pid /= 10; }
	while (i > 0) out[p++] = tmp[--i];
	out[p] = '\0';
}

static ssize_t print_uint(int fd, uint64_t n)
{
	char buf[32];
	size_t i = 0;
	if (n == 0) { char z = '0'; return write(fd, &z, 1); }
	while (n > 0) { buf[i++] = (char)('0' + (n % 10)); n /= 10; }
	for (size_t j = 0; j < i / 2; ++j) {
		char t = buf[j]; buf[j] = buf[i - 1 - j]; buf[i - 1 - j] = t;
	}
	return write(fd, buf, i);
}

static int create_shm(const char *name)
{
	int fd = shm_open(name, O_RDWR | O_CREAT | O_TRUNC, 0600);
	if (fd == -1) {
		const char msg[] = "error: failed to create SHM\n";
		write(STDERR_FILENO, msg, sizeof(msg) - 1);
		exit(EXIT_FAILURE);
	}
	if (ftruncate(fd, SHM_SIZE) == -1) {
		const char msg[] = "error: failed to resize SHM\n";
		write(STDERR_FILENO, msg, sizeof(msg) - 1);
		exit(EXIT_FAILURE);
	}
	return fd;
}

static char *map_shm(int fd)
{
	char *p = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE,
	               MAP_SHARED, fd, 0);
	if (p == MAP_FAILED) {
		const char msg[] = "error: failed to map SHM\n";
		write(STDERR_FILENO, msg, sizeof(msg) - 1);
		exit(EXIT_FAILURE);
	}
	return p;
}

static sem_t *create_sem(const char *name)
{
	sem_t *s = sem_open(name, O_RDWR | O_CREAT | O_TRUNC, 0600, 0);
	if (s == SEM_FAILED) {
		const char msg[] = "error: failed to create semaphore\n";
		write(STDERR_FILENO, msg, sizeof(msg) - 1);
		exit(EXIT_FAILURE);
	}
	return s;
}

static const char CHILD1_PROGRAM[] = "child1";
static const char CHILD2_PROGRAM[] = "child2";

int main(void)
{
	int ppid = (int)getpid();
	char pid_str[16];
	{
		int i = 0; int p = ppid;
		char tmp[12]; int j = 0;
		if (p == 0) tmp[j++] = '0';
		while (p > 0) { tmp[j++] = (char)('0' + p % 10); p /= 10; }
		while (j > 0) pid_str[i++] = tmp[--j];
		pid_str[i] = '\0';
	}

	char p2c1_shm_name[64], c1c2_shm_name[64], c2p_shm_name[64];
	char p2c1_sem_name[64], c1c2_sem_name[64], c2p_sem_name[64];

	make_name(p2c1_shm_name, "/labs-p2c1-",  ppid);
    make_name(c1c2_shm_name, "/labs-c1c2-",  ppid);
    make_name(c2p_shm_name,  "/labs-c2p-",   ppid);
    make_name(p2c1_sem_name, "/labs-sem-p-c1-", ppid);
    make_name(c1c2_sem_name, "/labs-sem-c1-c2-", ppid);
    make_name(c2p_sem_name,  "/labs-sem-c2-p-",  ppid);

	int p2c1_fd = create_shm(p2c1_shm_name);
	int c1c2_fd = create_shm(c1c2_shm_name);
	int c2p_fd  = create_shm(c2p_shm_name);

	char *p2c1_buf = map_shm(p2c1_fd);
	char *c1c2_buf = map_shm(c1c2_fd);
	char *c2p_buf  = map_shm(c2p_fd);

	*(uint32_t *)p2c1_buf = 0;
	*(uint32_t *)c1c2_buf = 0;
	*(uint32_t *)c2p_buf  = 0;

	sem_t *p2c1_sem = create_sem(p2c1_sem_name);
	sem_t *c1c2_sem = create_sem(c1c2_sem_name);
	sem_t *c2p_sem  = create_sem(c2p_sem_name);

	pid_t child1 = fork();
	if (child1 == 0) {
		char *args[] = { (char *)CHILD1_PROGRAM, pid_str, NULL };
		execv(CHILD1_PROGRAM, args);
		const char msg[] = "error: failed to exec child1\n";
		write(STDERR_FILENO, msg, sizeof(msg) - 1);
		exit(EXIT_FAILURE);
	} else if (child1 == -1) {
		const char msg[] = "error: failed to fork child1\n";
		write(STDERR_FILENO, msg, sizeof(msg) - 1);
		exit(EXIT_FAILURE);
	}

	pid_t child2 = fork();
	if (child2 == 0) {
		char *args[] = { (char *)CHILD2_PROGRAM, pid_str, NULL };
		execv(CHILD2_PROGRAM, args);
		const char msg[] = "error: failed to exec child2\n";
		write(STDERR_FILENO, msg, sizeof(msg) - 1);
		exit(EXIT_FAILURE);
	} else if (child2 == -1) {
		const char msg[] = "error: failed to fork child2\n";
		write(STDERR_FILENO, msg, sizeof(msg) - 1);
		exit(EXIT_FAILURE);
	}

	{
		const char m1[] = "Parent started.\nChild1 PID: ";
		write(STDOUT_FILENO, m1, sizeof(m1) - 1);
		print_uint(STDOUT_FILENO, (uint64_t)child1);
		const char m2[] = "\nChild2 PID: ";
		write(STDOUT_FILENO, m2, sizeof(m2) - 1);
		print_uint(STDOUT_FILENO, (uint64_t)child2);
		const char m3[] = "\nType text to get started (empty line to exit):\n";
		write(STDOUT_FILENO, m3, sizeof(m3) - 1);
	}

	bool running = true;
	while (running) {
		char buf[SHM_SIZE - sizeof(uint32_t)];
		ssize_t bytes = read(STDIN_FILENO, buf, sizeof(buf));
		if (bytes == -1) {
			const char msg[] = "error: failed to read stdin\n";
			write(STDERR_FILENO, msg, sizeof(msg) - 1);
			exit(EXIT_FAILURE);
		}

		bool eof = (bytes == 0 || buf[0] == '\n');

		{
			uint32_t *len = (uint32_t *)p2c1_buf;
			char     *txt = p2c1_buf + sizeof(uint32_t);
			if (eof) {
				*len = UINT32_MAX;
			} else {
				*len = (uint32_t)bytes;
				memcpy(txt, buf, (size_t)bytes);
			}
		}
		sem_post(p2c1_sem);

		if (eof) { running = false; break; }
		sem_wait(c2p_sem);
		{
			uint32_t *len = (uint32_t *)c2p_buf;
			char     *txt = c2p_buf + sizeof(uint32_t);
			if (*len != 0 && *len != UINT32_MAX) {
				const char pre[] = "Result: ";
				write(STDOUT_FILENO, pre, sizeof(pre) - 1);
				write(STDOUT_FILENO, txt, *len);
				write(STDOUT_FILENO, "\n", 1);
				*len = 0;
			}
		}
	}

	pid_t exitd1 = wait(NULL);
    {
        print_uint(STDOUT_FILENO, exitd1);
        const char msg[] = " exited.\n";
        write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    }
    pid_t exitd2 = wait(NULL);
    {
        print_uint(STDOUT_FILENO, exitd2);
        const char msg[] = " exited.\n";
        write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    }

	sem_unlink(p2c1_sem_name);
	sem_unlink(c1c2_sem_name);
	sem_unlink(c2p_sem_name);
	sem_close(p2c1_sem);
	sem_close(c1c2_sem);
	sem_close(c2p_sem);

	munmap(p2c1_buf, SHM_SIZE);
	munmap(c1c2_buf, SHM_SIZE);
	munmap(c2p_buf,  SHM_SIZE);
	shm_unlink(p2c1_shm_name);
	shm_unlink(c1c2_shm_name);
	shm_unlink(c2p_shm_name);
	close(p2c1_fd);
	close(c1c2_fd);
	close(c2p_fd);

	write(STDOUT_FILENO, "Parent exited.\n", 15);
	return 0;
}