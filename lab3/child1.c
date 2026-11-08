#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>

#define SHM_SIZE 4096

/* Собираем "/labs-c1in-<pid>" без snprintf. */
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

int main(int argc, char *argv[])
{
	if (argc != 2) {
		const char msg[] = "error: child1 expects parent pid argument\n";
		write(STDERR_FILENO, msg, sizeof(msg) - 1);
		_exit(EXIT_FAILURE);
	}
	int ppid = atoi(argv[1]);

	char in_shm_name[64],  out_shm_name[64];
	char in_sem_name[64],  out_sem_name[64];
	make_name(in_shm_name,  "/labs-p2c1-",  ppid);   
    make_name(out_shm_name, "/labs-c1c2-",  ppid);  
    make_name(in_sem_name,  "/labs-sp2c1-", ppid);
    make_name(out_sem_name, "/labs-sc1c2-", ppid);

	int in_shm = shm_open(in_shm_name, O_RDWR, 0);
	if (in_shm == -1) {
		const char msg[] = "error: failed to open input SHM\n";
		write(STDERR_FILENO, msg, sizeof(msg) - 1);
		_exit(EXIT_FAILURE);
	}
	int out_shm = shm_open(out_shm_name, O_RDWR, 0);
	if (out_shm == -1) {
		const char msg[] = "error: failed to open output SHM\n";
		write(STDERR_FILENO, msg, sizeof(msg) - 1);
		_exit(EXIT_FAILURE);
	}

	char *in_buf = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE,
	                    MAP_SHARED, in_shm, 0);
	if (in_buf == MAP_FAILED) {
		const char msg[] = "error: failed to map input SHM\n";
		write(STDERR_FILENO, msg, sizeof(msg) - 1);
		_exit(EXIT_FAILURE);
	}
	char *out_buf = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE,
	                     MAP_SHARED, out_shm, 0);
	if (out_buf == MAP_FAILED) {
		const char msg[] = "error: failed to map output SHM\n";
		write(STDERR_FILENO, msg, sizeof(msg) - 1);
		_exit(EXIT_FAILURE);
	}

	sem_t *in_sem = sem_open(in_sem_name, O_RDWR);
	if (in_sem == SEM_FAILED) {
		const char msg[] = "error: failed to open input semaphore\n";
		write(STDERR_FILENO, msg, sizeof(msg) - 1);
		_exit(EXIT_FAILURE);
	}
	sem_t *out_sem = sem_open(out_sem_name, O_RDWR);
	if (out_sem == SEM_FAILED) {
		const char msg[] = "error: failed to open output semaphore\n";
		write(STDERR_FILENO, msg, sizeof(msg) - 1);
		_exit(EXIT_FAILURE);
	}

	bool running = true;
	while (running) {
		sem_wait(in_sem);

		uint32_t *in_len  = (uint32_t *)in_buf;
		char     *in_text = in_buf + sizeof(uint32_t);

		uint32_t n = *in_len;
		if (n == UINT32_MAX) {
			running = false;
			*in_len = 0;
		} else if (n > 0) {
			/* трансформация: uppercase */
			for (uint32_t i = 0; i < n; ++i)
				in_text[i] = (char)toupper((unsigned char)in_text[i]);
			*in_len = 0;

			/* положим результат в выходной буфер */
			sem_wait(out_sem);
			uint32_t *out_len  = (uint32_t *)out_buf;
			char     *out_text = out_buf + sizeof(uint32_t);
			*out_len = n;
			memcpy(out_text, in_text, n);
			sem_post(out_sem);
		} else {
			*in_len = 0;
		}

		sem_post(in_sem);
	}

	/* пробрасываем EOF дальше в child2 */
	sem_wait(out_sem);
	uint32_t *out_len = (uint32_t *)out_buf;
	*out_len = UINT32_MAX;
	sem_post(out_sem);

	sem_close(in_sem);
	sem_close(out_sem);
	munmap(in_buf, SHM_SIZE);
	munmap(out_buf, SHM_SIZE);
	close(in_shm);
	close(out_shm);
	return 0;
}