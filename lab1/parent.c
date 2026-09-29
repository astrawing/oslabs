#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <signal.h>

static ssize_t print_uint(int fd, uint64_t n) {
    char buf[32];       
    size_t i = 0;

    if (n == 0) {
        char zero = '0';
        return write(fd, &zero, 1);
    }

    while (n > 0) {
        buf[i++] = (char)('0' + (n % 10));
        n /= 10;
    }

    for (size_t j = 0; j < i / 2; ++j) {
        char tmp = buf[j];
        buf[j] = buf[i - 1 - j];
        buf[i - 1 - j] = tmp;
    }

    return write(fd, buf, i);
}

static char CHILD1_PROGRAM[] = "child1";
static char CHILD2_PROGRAM[] = "child2";

int main(void) {
    int pipe1[2];         
    int pipe_between[2];  
    int pipe2[2];        

    if (pipe(pipe1) == -1) {
        const char msg[] = "error: failed to create pipe1\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }
    if (pipe(pipe_between) == -1) {
        const char msg[] = "error: failed to create pipe_between\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }
    if (pipe(pipe2) == -1) {
        const char msg[] = "error: failed to create pipe2\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    pid_t child1 = fork();
    if (child1 == -1) {
        const char msg[] = "error: fork child1 failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    if (child1 == 0) {
        close(pipe1[1]);
        close(pipe_between[0]);
        close(pipe2[0]);
        close(pipe2[1]);

        if (dup2(pipe1[0], STDIN_FILENO) == -1) {
            const char msg[] = "error: dup2 child1 stdin failed\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            exit(EXIT_FAILURE);
        }
        close(pipe1[0]);

        if (dup2(pipe_between[1], STDOUT_FILENO) == -1) {
            const char msg[] = "error: dup2 child1 stdout failed\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            exit(EXIT_FAILURE);
        }
        close(pipe_between[1]);

        char *const args[] = {CHILD1_PROGRAM, NULL};
        execv(CHILD1_PROGRAM, args);

        const char msg[] = "error: failed to exec child1\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    pid_t child2 = fork();
    if (child2 == -1) {
        const char msg[] = "error: fork child2 failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        waitpid(child1, NULL, 0);
        exit(EXIT_FAILURE);
    }

    if (child2 == 0) {
        close(pipe1[0]);
        close(pipe1[1]);
        close(pipe_between[1]);
        close(pipe2[0]);

        if (dup2(pipe_between[0], STDIN_FILENO) == -1) {
            const char msg[] = "error: dup2 child2 stdin failed\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            exit(EXIT_FAILURE);
        }
        close(pipe_between[0]);

        if (dup2(pipe2[1], STDOUT_FILENO) == -1) {
            const char msg[] = "error: dup2 child2 stdout failed\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            exit(EXIT_FAILURE);
        }
        close(pipe2[1]);

        char *const args[] = {CHILD2_PROGRAM, NULL};
        execv(CHILD2_PROGRAM, args);

        const char msg[] = "error: failed to exec child2\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    close(pipe1[0]);
    close(pipe_between[0]);
    close(pipe_between[1]);
    close(pipe2[1]);

    {
        const char msg[] = "Parent started.\nChild1 PID: ";
        write(STDOUT_FILENO, msg, sizeof(msg) - 1);
        print_uint(STDOUT_FILENO, child1);
        const char msg2[] = "\nChild2 PID: ";
        write(STDOUT_FILENO, msg2, sizeof(msg2) - 1);
        print_uint(STDOUT_FILENO, child2);
        const char msg3[] = "\nType text to get started (empty line to exit):\n";
        write(STDOUT_FILENO, msg3, sizeof(msg3) - 1);
    }

    char buf[4096];
    ssize_t bytes;

    while (1) {
        bytes = read(STDIN_FILENO, buf, sizeof(buf));
        if (bytes < 0) {
            const char msg[] = "error: parent failed to read stdin\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            break;
        }
        if (bytes == 0 || buf[0] == '\n') break; 

        ssize_t written = write(pipe1[1], buf, bytes);
        if (written != bytes) {
            const char msg[] = "error: parent failed to write to pipe1 (data limited to 4096 bytes)\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            break;
        }

        bytes = read(pipe2[0], buf, sizeof(buf));
        if (bytes < 0) {
            const char msg[] = "error: parent failed to read from pipe2\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            break;
        }

        if (bytes > 0) {
            {
                const char msg[] = "Result: ";
                write(STDOUT_FILENO, msg, sizeof(msg) - 1);
            }

            written = write(STDOUT_FILENO, buf, (size_t)bytes);
            if (written != bytes) {
                const char msg[] = "error: parent failed to write to stdout (data limited to 4096 bytes)\n";
                write(STDERR_FILENO, msg, sizeof(msg) - 1);
                break;
            }

            {
                const char msg[] = "\n";
                write(STDOUT_FILENO, msg, sizeof(msg) - 1);
            }
        }
    }

    close(pipe1[1]);
    close(pipe2[0]);

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

    {
        const char msg[] = "Parent exited.\n";
        write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    }

    return 0;
}