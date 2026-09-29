#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

int main(void) {
    char buf[4096];
    ssize_t bytes;

    while ((bytes = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
        for (uint32_t i = 0; i < bytes; ++i) {
            buf[i] = toupper(buf[i]);
        }

        ssize_t written = write(STDOUT_FILENO, buf, (size_t)bytes);
        if (written != bytes) {
            const char msg[] = "error: child1 failed to write.\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            exit(EXIT_FAILURE);
        }
    }

    if (bytes < 0) {
        const char msg[] = "error: child1 failed to read.\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    return 0;
}