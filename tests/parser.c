
#include "parse.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_simple(void) {
    job j;
    assert(parse_job("echo hello", &j) == 1);
    assert(j.n == 1);
    assert(j.cmds[0].argc == 2);
    assert(strcmp(j.cmds[0].argv[0], "echo") == 0);
    assert(strcmp(j.cmds[0].argv[1], "hello") == 0);
    assert(j.cmds[0].argv[2] == NULL);
    free_job(&j);
}

static void test_redirect(void) {
    job j;
    assert(parse_job("cat < input.txt >> output.txt", &j) == 1);
    assert(j.n == 1);
    assert(strcmp(j.cmds[0].infile, "input.txt") == 0);
    assert(strcmp(j.cmds[0].outfile, "output.txt") == 0);
    assert(j.cmds[0].append == 1);
    assert(j.cmds[0].argc == 1);
    free_job(&j);
}

static void test_pipe(void) {
    job j;
    assert(parse_job("cat file | grep hello | wc -l", &j) == 1);
    assert(j.n == 3);
    assert(j.cmds[0].argc == 2);
    assert(j.cmds[1].argc == 2);
    assert(j.cmds[2].argc == 2);
    assert(strcmp(j.cmds[2].argv[0], "wc") == 0);
    free_job(&j);
}

static void test_invalid(void) {
    const char *cases[] = {
        "| echo hello",
        "echo hello |",
        "echo | | wc",
        "cat <",
        "cat < < input.txt",
        "echo >",
        "> output.txt"
    };

    for (size_t i = 0;
         i < sizeof(cases) / sizeof(cases[0]);
         i++) {
        job j;
        assert(parse_job(cases[i], &j) == -1);
        assert(j.cmds == NULL);
    }
}

int main(void) {
    test_simple();
    test_redirect();
    test_pipe();
    test_invalid();

    job j;
    assert(parse_job(" # just a comment", &j) == 0);
    assert(j.cmds == NULL);

    puts("Parser: all initial tests passed");
    return 0;
}
