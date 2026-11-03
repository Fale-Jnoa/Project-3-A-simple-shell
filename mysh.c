
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>


static void handle_line(const char *line) {
#ifdef DEBUG_INPUT
    printf("READ: [%s]\n", line);
#else
    (void)line;
#endif
}

static void read_commands(int input_fd) {
    size_t capacity = 128;
    size_t length = 0;

    char *buffer = malloc(capacity);

    if (buffer == NULL) {
        perror("mysh: malloc");
        return;
    }

    while (1) {
        char c;
        ssize_t bytes_read = read(input_fd, &c, 1);

        if (bytes_read == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("mysh: read");
            break;
        }

        if (bytes_read == 0) {
            if (length > 0) {
                buffer[length] = '\0';
                handle_line(buffer);
            }
            break;
        }

        if (c == '\n') {
            buffer[length] = '\0';
            handle_line(buffer);
            length = 0;
            continue;
        }

        if (length + 1 >= capacity) {
            if (capacity > (size_t)-1 / 2) {
                fprintf(stderr, "mysh: input too long\n");
                break;
            }

            size_t new_capacity = capacity * 2;
            char *temp = realloc(buffer, new_capacity);

            if (temp == NULL) {
                perror("mysh: realloc");
                break;
            }

            buffer = temp;
            capacity = new_capacity;
        }

        buffer[length++] = c;
    }

    free(buffer);
}

int main(int argc, char *argv[]) {
    int input_fd = STDIN_FILENO;
    int interactive = 0;

    if (argc > 2) {
        fprintf(stderr, "mysh: too many arguments\n");
        return EXIT_SUCCESS;
    }

    if (argc == 2) {
        input_fd = open(argv[1], O_RDONLY);

        if (input_fd == -1) {
            perror("mysh");
            return EXIT_FAILURE;
        }
    } else {
        interactive = isatty(STDIN_FILENO);
    }

    if (interactive) {
        printf("Welcome to my shell!\n");
    }

    read_commands(input_fd);

    if (input_fd != STDIN_FILENO) {
        close(input_fd);
    }

    if (interactive) {
        printf("Exiting my shell.\n");
    }

    return EXIT_SUCCESS;
}
