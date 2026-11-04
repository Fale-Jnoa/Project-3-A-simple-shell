#define _POSIX_C_SOURCE 200809L

#include "parse.h"

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void make_file(const char *path) {
    int fd = open(path, O_CREAT | O_EXCL | O_WRONLY, 0600);
    assert(fd >= 0);
    assert(close(fd) == 0);
}

static void check_arg(const subcmd *cmd, int index, const char *expected) {
    assert(index >= 0 && index < cmd->argc);
    assert(strcmp(cmd->argv[index], expected) == 0);
}

int main(void) {
    char dir[] = "/tmp/p3_job_glob_XXXXXX";
    assert(mkdtemp(dir) != NULL);

    char alpha[512], beta[512], hidden[512], foo[512], foobar[512];
    char missing[512], redirect[512], line[2048];

    snprintf(alpha, sizeof alpha, "%s/alpha.txt", dir);
    snprintf(beta, sizeof beta, "%s/beta.txt", dir);
    snprintf(hidden, sizeof hidden, "%s/.hidden.txt", dir);
    snprintf(foo, sizeof foo, "%s/foo123bar", dir);
    snprintf(foobar, sizeof foobar, "%s/foobar", dir);
    snprintf(missing, sizeof missing, "%s/*.xyz", dir);
    snprintf(redirect, sizeof redirect, "%s/*.output", dir);

    make_file(alpha);
    make_file(beta);
    make_file(hidden);
    make_file(foo);
    make_file(foobar);

    job j;

    // Two wildcard arguments expand independently, and unmatched patterns survive.
    snprintf(line, sizeof line, "echo %s/*.txt %s/foo*bar %s", dir, dir, missing);
    assert(parse_job(line, &j) == 1);
    assert(j.n == 1);
    assert(j.cmds[0].argc == 6);
    check_arg(&j.cmds[0], 0, "echo");
    check_arg(&j.cmds[0], 1, alpha);
    check_arg(&j.cmds[0], 2, beta);
    check_arg(&j.cmds[0], 3, foo);
    check_arg(&j.cmds[0], 4, foobar);
    check_arg(&j.cmds[0], 5, missing);
    assert(j.cmds[0].argv[6] == NULL);
    free_job(&j);
    assert(j.cmds == NULL && j.n == 0);

    // Wildcards expand per pipeline stage, not into the pipe symbol or second argv.
    snprintf(line, sizeof line, "echo %s/*.txt | wc -w", dir);
    assert(parse_job(line, &j) == 1);
    assert(j.n == 2);
    assert(j.cmds[0].argc == 3);
    check_arg(&j.cmds[0], 1, alpha);
    check_arg(&j.cmds[0], 2, beta);
    assert(j.cmds[1].argc == 2);
    check_arg(&j.cmds[1], 0, "wc");
    check_arg(&j.cmds[1], 1, "-w");
    free_job(&j);

    // A wildcard in a redirection filename must not be expanded.
    snprintf(line, sizeof line, "echo %s/*.txt > %s", dir, redirect);
    assert(parse_job(line, &j) == 1);
    assert(j.cmds[0].argc == 3);
    assert(strcmp(j.cmds[0].outfile, redirect) == 0);
    assert(j.cmds[0].append == 0);
    free_job(&j);

    // An explicitly named hidden-file prefix may match a hidden file.
    snprintf(line, sizeof line, "echo %s/.hidden*", dir);
    assert(parse_job(line, &j) == 1);
    assert(j.cmds[0].argc == 2);
    check_arg(&j.cmds[0], 1, hidden);
    free_job(&j);

    assert(unlink(alpha) == 0);
    assert(unlink(beta) == 0);
    assert(unlink(hidden) == 0);
    assert(unlink(foo) == 0);
    assert(unlink(foobar) == 0);
    assert(rmdir(dir) == 0);

    puts("Wildcard-to-job integration: all tests passed");
    return 0;
}
