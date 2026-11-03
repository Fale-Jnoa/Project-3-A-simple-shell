
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>


static void handle_line(const char *line) {
#ifdef DEBUG_INPUT
    printf("READ: [%s]\n", line);
#else
    (void)line;
#endif
}


static void print_prompt(void) {
    size_t capacity = 128;
    char *cwd = malloc(capacity);

    if (cwd == NULL) {
        perror("mysh: malloc");
        return;
    }

    while (getcwd(cwd, capacity) == NULL) {
        if (errno != ERANGE) {
            perror("mysh: getcwd");
            free(cwd);
            return;
        }

        if (capacity > SIZE_MAX / 2) {
            fprintf(stderr, "mysh: path too long\n");
            free(cwd);
            return;
        }

        capacity *= 2;
        char *temp = realloc(cwd, capacity);

        if (temp == NULL) {
            perror("mysh: realloc");
            free(cwd);
            return;
        }

        cwd = temp;
    }

    const char *home = getenv("HOME");
    size_t home_len = home ? strlen(home) : 0;

    if (home_len > 0 &&
        strncmp(cwd, home, home_len) == 0 &&
        (cwd[home_len] == '/' || cwd[home_len] == '\0')) {
        printf("~%s$ ", cwd + home_len);
    } else {
        printf("%s$ ", cwd);
    }

    fflush(stdout);
    free(cwd);
}

static void read_commands(int input_fd, int interactive) {
    size_t capacity = 128;
    size_t length = 0;

    char *buffer = malloc(capacity);

    if (buffer == NULL) {
        perror("mysh: malloc");
        return;
    }

    int needs_prompt = interactive;

    while (1) {
        char c;
        if (needs_prompt) {
            print_prompt();
            needs_prompt = 0;
        }
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
            needs_prompt = interactive;
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

    read_commands(input_fd, interactive);

    if (input_fd != STDIN_FILENO) {
        close(input_fd);
    }

    if (interactive) {
        printf("Exiting my shell.\n");
    }

    return EXIT_SUCCESS;
}
