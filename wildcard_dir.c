
#define _POSIX_C_SOURCE 200809L

#include "parse.h"

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void make_file(const char *path) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    assert(fd >= 0);
    assert(close(fd) == 0);
}

int main(void) {
    char dir[] = "/tmp/p3_glob_XXXXXX";
    assert(mkdtemp(dir) != NULL);

    char alpha[512], beta[512];
    char hidden[512], other[512], pattern[512];

    snprintf(alpha, sizeof(alpha), "%s/alpha.txt", dir);
    snprintf(beta, sizeof(beta), "%s/beta.txt", dir);
    snprintf(hidden, sizeof(hidden), "%s/.secret.txt", dir);
    snprintf(other, sizeof(other), "%s/notes.c", dir);

    make_file(alpha);
    make_file(beta);
    make_file(hidden);
    make_file(other);

    char **matches;
    size_t count;

    // Match visible .txt files.
    snprintf(pattern, sizeof(pattern), "%s/*.txt", dir);

    assert(collect_wildcard_matches(pattern, &matches, &count) == 0);
    assert(count == 2);
    assert(strcmp(matches[0], alpha) == 0);
    assert(strcmp(matches[1], beta) == 0);
    free_wildcard_matches(matches, count);

    // Explicitly match a hidden file.
    snprintf(pattern, sizeof(pattern), "%s/.secret*", dir);

    assert(collect_wildcard_matches(pattern, &matches, &count) == 0);
    assert(count == 1);
    assert(strcmp(matches[0], hidden) == 0);
    free_wildcard_matches(matches, count);

    // No matching files.
    snprintf(pattern, sizeof(pattern), "%s/*.xyz", dir);

    assert(collect_wildcard_matches(pattern, &matches, &count) == 0);
    assert(count == 0);
    free_wildcard_matches(matches, count);

    // Clean up the temporary test directory.
    assert(unlink(alpha) == 0);
    assert(unlink(beta) == 0);
    assert(unlink(hidden) == 0);
    assert(unlink(other) == 0);
    assert(rmdir(dir) == 0);

    puts("Wildcard directory scanning: all tests passed");
    return 0;
}

