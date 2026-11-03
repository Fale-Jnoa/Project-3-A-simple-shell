
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

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

    // TODO: Add command-reading loop here.

    if (input_fd != STDIN_FILENO) {
        close(input_fd);
    }

    if (interactive) {
        printf("Exiting my shell.\n");
    }

    return EXIT_SUCCESS;
}
