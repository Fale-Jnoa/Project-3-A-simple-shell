// Unit tests for exec.c. Builds job structs by hand (no parser involved)
// and checks run_job's return value plus any files or output it makes.
// Run with: make exec_test && ./exec_test

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "job.h"

static int tests_run = 0;

// Makes a job with one stage. argv must be NULL terminated.
static job one(char **argv, char *in, char *out, int append) {
    static subcmd cmd;
    int argc = 0;
    while (argv[argc] != NULL) {
        argc++;
    }
    cmd = (subcmd){ argv, argc, in, out, append };
    return (job){ &cmd, 1 };
}

// Makes a pipeline from an array of stages.
static job pipeline(subcmd *cmds, int n) {
    for (int i = 0; i < n; i++) {
        int argc = 0;
        while (cmds[i].argv[argc] != NULL) {
            argc++;
        }
        cmds[i].argc = argc;
    }
    return (job){ cmds, n };
}

#define ARGS(...) ((char *[]){ __VA_ARGS__, NULL })
#define STAGE(...) ((subcmd){ ARGS(__VA_ARGS__), 0, NULL, NULL, 0 })

// Reads a whole (small) file into a static buffer.
static const char *slurp(const char *path) {
    static char buf[4096];
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        return NULL;
    }
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    buf[n < 0 ? 0 : n] = '\0';
    return buf;
}

// Runs a job with the shell's own stdout sent to a file, so we can see
// the "Exited with status" style messages. Returns run_job's result.
static int run_capturing(job *j, int interactive, const char *capture) {
    fflush(stdout);
    int saved = dup(STDOUT_FILENO);
    int fd = open(capture, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    assert(saved != -1 && fd != -1);
    dup2(fd, STDOUT_FILENO);
    close(fd);

    int result = run_job(j, interactive);

    fflush(stdout);
    dup2(saved, STDOUT_FILENO);
    close(saved);
    return result;
}

#define CHECK(cond) do { assert(cond); tests_run++; } while (0)

static void test_programs(void) {
    job j;

    j = one(ARGS("true"), NULL, NULL, 0);
    CHECK(run_job(&j, 0) == 1);

    j = one(ARGS("false"), NULL, NULL, 0);
    CHECK(run_job(&j, 0) == 0);

    // nonzero exit code that isn't 1
    j = one(ARGS("sh", "-c", "exit 7"), NULL, NULL, 0);
    CHECK(run_job(&j, 0) == 0);

    j = one(ARGS("definitely_not_a_program_xyz"), NULL, NULL, 0);
    CHECK(run_job(&j, 0) == 0);

    // names with a slash skip the search
    j = one(ARGS("/bin/true"), NULL, NULL, 0);
    CHECK(run_job(&j, 0) == 1);

    j = one(ARGS("./no_such_file"), NULL, NULL, 0);
    CHECK(run_job(&j, 0) == 0);

    // a file that exists but isn't executable
    int fd = open("plain.txt", O_WRONLY | O_CREAT | O_TRUNC, 0600);
    close(fd);
    j = one(ARGS("./plain.txt"), NULL, NULL, 0);
    CHECK(run_job(&j, 0) == 0);
}

static void test_redirection(void) {
    job j;
    struct stat sb;

    j = one(ARGS("echo", "hello"), NULL, "out.txt", 0);
    CHECK(run_job(&j, 0) == 1);
    CHECK(strcmp(slurp("out.txt"), "hello\n") == 0);

    // new files get mode 640
    CHECK(stat("out.txt", &sb) == 0);
    CHECK((sb.st_mode & 0777) == 0640);

    // >> appends
    j = one(ARGS("echo", "again"), NULL, "out.txt", 1);
    CHECK(run_job(&j, 0) == 1);
    CHECK(strcmp(slurp("out.txt"), "hello\nagain\n") == 0);

    // > truncates
    j = one(ARGS("echo", "new"), NULL, "out.txt", 0);
    CHECK(run_job(&j, 0) == 1);
    CHECK(strcmp(slurp("out.txt"), "new\n") == 0);

    // >> creates the file if it is missing
    j = one(ARGS("echo", "first"), NULL, "append_new.txt", 1);
    CHECK(run_job(&j, 0) == 1);
    CHECK(strcmp(slurp("append_new.txt"), "first\n") == 0);

    // < and > together
    j = one(ARGS("cat"), "out.txt", "copy.txt", 0);
    CHECK(run_job(&j, 0) == 1);
    CHECK(strcmp(slurp("copy.txt"), "new\n") == 0);

    // missing input file fails the command
    j = one(ARGS("cat"), "missing.txt", NULL, 0);
    CHECK(run_job(&j, 0) == 0);

    // output file we can't create fails the command
    j = one(ARGS("echo", "x"), NULL, "no_dir/out.txt", 0);
    CHECK(run_job(&j, 0) == 0);

    // command still fails if program exits nonzero after redirecting
    j = one(ARGS("cat", "missing.txt"), NULL, "err_out.txt", 0);
    CHECK(run_job(&j, 0) == 0);
}

static void test_pipes(void) {
    job j;

    // success depends only on the last stage
    subcmd a[] = { STAGE("false"), STAGE("true") };
    j = pipeline(a, 2);
    CHECK(run_job(&j, 0) == 1);

    subcmd b[] = { STAGE("true"), STAGE("false") };
    j = pipeline(b, 2);
    CHECK(run_job(&j, 0) == 0);

    // data actually goes through, with a redirect on the last stage
    subcmd c[] = { STAGE("echo", "a", "b", "c"), STAGE("wc", "-w") };
    c[1].outfile = "wc.txt";
    j = pipeline(c, 2);
    CHECK(run_job(&j, 0) == 1);
    CHECK(atoi(slurp("wc.txt")) == 3);

    // three stages
    subcmd d[] = { STAGE("echo", "hello"), STAGE("cat"), STAGE("cat") };
    d[2].outfile = "three.txt";
    j = pipeline(d, 3);
    CHECK(run_job(&j, 0) == 1);
    CHECK(strcmp(slurp("three.txt"), "hello\n") == 0);

    // lots of data, would hang if any write end was left open
    subcmd e[] = { STAGE("seq", "1", "200000"), STAGE("cat"), STAGE("wc", "-l") };
    e[2].outfile = "big.txt";
    j = pipeline(e, 3);
    CHECK(run_job(&j, 0) == 1);
    CHECK(atoi(slurp("big.txt")) == 200000);

    // unknown program anywhere in the pipeline fails it
    subcmd f[] = { STAGE("not_a_real_program_xyz"), STAGE("cat") };
    j = pipeline(f, 2);
    CHECK(run_job(&j, 0) == 0);
}

static void test_builtins(void) {
    job j;
    char start[PATH_MAX], now[PATH_MAX], expect[PATH_MAX + 16];
    assert(getcwd(start, sizeof(start)) != NULL);

    // pwd with redirect writes to the file, not the terminal
    j = one(ARGS("pwd"), NULL, "pwd.txt", 0);
    CHECK(run_job(&j, 0) == 1);
    snprintf(expect, sizeof(expect), "%s\n", start);
    CHECK(strcmp(slurp("pwd.txt"), expect) == 0);

    j = one(ARGS("pwd", "extra"), NULL, NULL, 0);
    CHECK(run_job(&j, 0) == 0);

    // cd into a subdirectory and back
    mkdir("sub", 0700);
    j = one(ARGS("cd", "sub"), NULL, NULL, 0);
    CHECK(run_job(&j, 0) == 1);
    getcwd(now, sizeof(now));
    snprintf(expect, sizeof(expect), "%s/sub", start);
    CHECK(strcmp(now, expect) == 0);

    j = one(ARGS("cd", ".."), NULL, NULL, 0);
    CHECK(run_job(&j, 0) == 1);
    getcwd(now, sizeof(now));
    CHECK(strcmp(now, start) == 0);

    // errors
    j = one(ARGS("cd", "nope_dir"), NULL, NULL, 0);
    CHECK(run_job(&j, 0) == 0);
    j = one(ARGS("cd", "a", "b"), NULL, NULL, 0);
    CHECK(run_job(&j, 0) == 0);

    // cd with no argument goes to HOME
    setenv("HOME", start, 1);
    chdir("sub");
    j = one(ARGS("cd"), NULL, NULL, 0);
    CHECK(run_job(&j, 0) == 1);
    getcwd(now, sizeof(now));
    CHECK(strcmp(now, start) == 0);

    // cd in a pipeline runs in a child and doesn't move the shell
    subcmd p[] = { STAGE("cd", "sub"), STAGE("cat") };
    j = pipeline(p, 2);
    CHECK(run_job(&j, 0) == 1);
    getcwd(now, sizeof(now));
    CHECK(strcmp(now, start) == 0);

    // pwd as the first stage of a pipeline
    subcmd q[] = { STAGE("pwd"), STAGE("cat") };
    q[1].outfile = "pwd_pipe.txt";
    j = pipeline(q, 2);
    CHECK(run_job(&j, 0) == 1);
    snprintf(expect, sizeof(expect), "%s\n", start);
    CHECK(strcmp(slurp("pwd_pipe.txt"), expect) == 0);

    // which
    j = one(ARGS("which"), NULL, NULL, 0);
    CHECK(run_job(&j, 0) == 0);

    j = one(ARGS("which", "true"), NULL, "which.txt", 0);
    CHECK(run_job(&j, 0) == 1);
    const char *w = slurp("which.txt");
    CHECK(strcmp(w, "/usr/local/bin/true\n") == 0 ||
          strcmp(w, "/usr/bin/true\n") == 0 ||
          strcmp(w, "/bin/true\n") == 0);

    j = one(ARGS("which", "no_such_prog_xyz"), NULL, "which2.txt", 0);
    CHECK(run_job(&j, 0) == 0);
    CHECK(strcmp(slurp("which2.txt"), "") == 0);

    // one found, one missing: prints the found one, still fails
    j = one(ARGS("which", "true", "no_such_prog_xyz"), NULL, "which3.txt", 0);
    CHECK(run_job(&j, 0) == 0);
    CHECK(strstr(slurp("which3.txt"), "/true\n") != NULL);

    // slash paths are printed as-is if executable
    j = one(ARGS("which", "/bin/sh"), NULL, "which4.txt", 0);
    CHECK(run_job(&j, 0) == 1);
    CHECK(strcmp(slurp("which4.txt"), "/bin/sh\n") == 0);

    // which ignores built-in names and only searches the directories,
    // so which pwd finds the coreutils program, not the built-in
    j = one(ARGS("which", "pwd"), NULL, "which5.txt", 0);
    CHECK(run_job(&j, 0) == 1);
    w = slurp("which5.txt");
    CHECK(w[0] == '/' && strstr(w, "/pwd\n") != NULL);

    // exit does nothing and succeeds, alone or in a pipeline
    j = one(ARGS("exit", "ignored", "args"), NULL, NULL, 0);
    CHECK(run_job(&j, 0) == 1);

    subcmd r[] = { STAGE("exit"), STAGE("echo", "hello") };
    r[1].outfile = "exit_pipe.txt";
    j = pipeline(r, 2);
    CHECK(run_job(&j, 0) == 1);
    CHECK(strcmp(slurp("exit_pipe.txt"), "hello\n") == 0);
}

static void test_status_messages(void) {
    job j;

    // interactive: nonzero exit is reported
    j = one(ARGS("sh", "-c", "exit 3"), NULL, NULL, 0);
    CHECK(run_capturing(&j, 1, "status.txt") == 0);
    CHECK(strcmp(slurp("status.txt"), "Exited with status 3\n") == 0);

    // interactive: killed by a signal
    j = one(ARGS("sh", "-c", "kill -9 $$"), NULL, NULL, 0);
    CHECK(run_capturing(&j, 1, "status.txt") == 0);
    CHECK(strncmp(slurp("status.txt"), "Terminated by signal ", 21) == 0);

    // interactive: success prints nothing
    j = one(ARGS("true"), NULL, NULL, 0);
    CHECK(run_capturing(&j, 1, "status.txt") == 1);
    CHECK(strcmp(slurp("status.txt"), "") == 0);

    // batch mode never prints the message
    j = one(ARGS("sh", "-c", "exit 3"), NULL, NULL, 0);
    CHECK(run_capturing(&j, 0, "status.txt") == 0);
    CHECK(strcmp(slurp("status.txt"), "") == 0);

    // pipeline reports the last stage only
    subcmd p[] = { STAGE("false"), STAGE("sh", "-c", "exit 4") };
    j = pipeline(p, 2);
    CHECK(run_capturing(&j, 1, "status.txt") == 0);
    CHECK(strcmp(slurp("status.txt"), "Exited with status 4\n") == 0);
}

// In batch mode a child's stdin is /dev/null, so it can't read the
// commands meant for the shell.
static void test_batch_stdin(void) {
    int p[2];
    assert(pipe(p) == 0);
    assert(write(p[1], "secret\n", 7) == 7);
    close(p[1]);

    int saved = dup(STDIN_FILENO);
    dup2(p[0], STDIN_FILENO);
    close(p[0]);

    job j = one(ARGS("cat"), NULL, "stdin.txt", 0);
    CHECK(run_job(&j, 0) == 1);
    CHECK(strcmp(slurp("stdin.txt"), "") == 0);

    // the data is still there for the shell to read
    char buf[16] = {0};
    CHECK(read(STDIN_FILENO, buf, sizeof(buf) - 1) == 7);
    CHECK(strcmp(buf, "secret\n") == 0);

    dup2(saved, STDIN_FILENO);
    close(saved);
}

int main(void) {
    char dir[] = "/tmp/exec_test_XXXXXX";
    char start[PATH_MAX];
    assert(getcwd(start, sizeof(start)) != NULL);
    assert(mkdtemp(dir) != NULL);
    assert(chdir(dir) == 0);
    umask(022);

    // if a pipeline hangs, die instead of waiting forever
    alarm(20);

    test_programs();
    test_redirection();
    test_pipes();
    test_builtins();
    test_status_messages();
    test_batch_stdin();

    chdir(start);
    char cmd[PATH_MAX + 16];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", dir);
    system(cmd);

    printf("exec_test: all %d checks passed\n", tests_run);
    return 0;
}
