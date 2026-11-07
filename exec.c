#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "job.h"

// Directories searched for bare program names, in order.
static const char *search_dirs[] = { "/usr/local/bin", "/usr/bin", "/bin", NULL };

// Mode for files created by > and >> (rw-r-----).
#define OUT_MODE (S_IRUSR | S_IWUSR | S_IRGRP)

// ---------- Program lookup ----------

static int is_builtin(const char *name) {
    return strcmp(name, "cd") == 0 ||
           strcmp(name, "pwd") == 0 ||
           strcmp(name, "which") == 0 ||
           strcmp(name, "exit") == 0;
}

// Returns an allocated executable path, or NULL if not found.
static char *search_path(const char *name) {
    size_t name_len = strlen(name);

    for (int i = 0; search_dirs[i] != NULL; i++) {
        size_t dir_len = strlen(search_dirs[i]);
        char *path = malloc(dir_len + 1 + name_len + 1);
        if (path == NULL) {
            perror("mysh: malloc");
            return NULL;
        }

        memcpy(path, search_dirs[i], dir_len);
        path[dir_len] = '/';
        memcpy(path + dir_len + 1, name, name_len + 1);

        if (access(path, X_OK) == 0) {
            return path;
        }
        free(path);
    }

    return NULL;
}

// Paths containing '/' bypass the search directories.
static char *find_program(const char *name) {
    if (strchr(name, '/') != NULL) {
        if (access(name, X_OK) != 0) {
            fprintf(stderr, "mysh: %s: %s\n", name, strerror(errno));
            return NULL;
        }

        char *copy = strdup(name);
        if (copy == NULL) {
            perror("mysh: strdup");
        }
        return copy;
    }

    char *path = search_path(name);
    if (path == NULL) {
        fprintf(stderr, "mysh: %s: command not found\n", name);
    }
    return path;
}

/* ---------- Built-in commands ---------- */
// Each built-in returns 1 on success and 0 on failure. Output goes to
// stdout, which may have been redirected before the call.

static int builtin_cd(subcmd *cmd) {
    const char *target;

    if (cmd->argc > 2) {
        fprintf(stderr, "cd: too many arguments\n");
        return 0;
    }

    if (cmd->argc == 1) {
        target = getenv("HOME");
        if (target == NULL) {
            fprintf(stderr, "cd: HOME not set\n");
            return 0;
        }
    } else {
        target = cmd->argv[1];
    }

    if (chdir(target) != 0) {
        perror("cd");
        return 0;
    }
    return 1;
}

static int builtin_pwd(subcmd *cmd) {
    if (cmd->argc > 1) {
        fprintf(stderr, "pwd: too many arguments\n");
        return 0;
    }

    size_t size = 128;
    char *buf = malloc(size);
    if (buf == NULL) {
        perror("pwd");
        return 0;
    }

    // getcwd fails with ERANGE if the buffer is too small, so keep growing it
    while (getcwd(buf, size) == NULL) {
        if (errno != ERANGE) {
            perror("pwd");
            free(buf);
            return 0;
        }

        size *= 2;
        char *temp = realloc(buf, size);
        if (temp == NULL) {
            perror("pwd");
            free(buf);
            return 0;
        }
        buf = temp;
    }

    printf("%s\n", buf);
    free(buf);
    return 1;
}

// which does not treat built-ins specially, it only does the path search.
static int builtin_which(subcmd *cmd) {
    if (cmd->argc < 2) {
        return 0;
    }

    int found_all = 1;

    for (int i = 1; i < cmd->argc; i++) {
        const char *name = cmd->argv[i];

        if (strchr(name, '/') != NULL) {
            if (access(name, X_OK) == 0) {
                printf("%s\n", name);
            } else {
                found_all = 0;
            }
            continue;
        }

        char *path = search_path(name);
        if (path != NULL) {
            printf("%s\n", path);
            free(path);
        } else {
            found_all = 0;
        }
    }

    return found_all;
}

static int run_builtin(subcmd *cmd) {
    const char *name = cmd->argv[0];
    int result;

    if (strcmp(name, "cd") == 0) {
        result = builtin_cd(cmd);
    } else if (strcmp(name, "pwd") == 0) {
        result = builtin_pwd(cmd);
    } else if (strcmp(name, "which") == 0) {
        result = builtin_which(cmd);
    } else {
        // exit ignores its arguments. mysh.c sees it and stops the loop.
        result = 1;
    }

    fflush(stdout);
    return result;
}

/* ---------- Redirection ---------- */

// Opens the redirect files for one subcommand. Unused ones are left at -1.
// On failure, prints an error, closes anything it opened, and returns 0.
static int open_redirects(subcmd *cmd, int *in_fd, int *out_fd) {
    *in_fd = -1;
    *out_fd = -1;

    if (cmd->infile != NULL) {
        *in_fd = open(cmd->infile, O_RDONLY);
        if (*in_fd == -1) {
            fprintf(stderr, "mysh: %s: %s\n", cmd->infile, strerror(errno));
            return 0;
        }
    }

    if (cmd->outfile != NULL) {
        int flags = O_WRONLY | O_CREAT | (cmd->append ? O_APPEND : O_TRUNC);
        *out_fd = open(cmd->outfile, flags, OUT_MODE);
        if (*out_fd == -1) {
            fprintf(stderr, "mysh: %s: %s\n", cmd->outfile, strerror(errno));
            if (*in_fd != -1) {
                close(*in_fd);
                *in_fd = -1;
            }
            return 0;
        }
    }

    return 1;
}

static void close_if_open(int fd) {
    if (fd != -1) {
        close(fd);
    }
}

/* ---------- Reporting child status ---------- */

// Returns 1 if the child exited normally with status 0. In interactive
// mode, also tells the user when the command failed or was killed.
static int check_status(int status, int interactive) {
    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        if (code != 0 && interactive) {
            printf("Exited with status %d\n", code);
            fflush(stdout);
        }
        return code == 0;
    }

    if (WIFSIGNALED(status) && interactive) {
        printf("Terminated by signal %s\n", strsignal(WTERMSIG(status)));
        fflush(stdout);
    }
    return 0;
}

/* ---------- Running a single built-in in the shell ---------- */

// Built-ins with no pipe run inside mysh itself so cd actually changes
// the shell's directory. If there is a > or >> redirect, stdout is
// pointed at the file for the call and then put back.
static int run_builtin_in_shell(subcmd *cmd) {
    int in_fd, out_fd;
    if (!open_redirects(cmd, &in_fd, &out_fd)) {
        return 0;
    }

    // Built-ins do not read stdin, so the input file can be closed.
    close_if_open(in_fd);

    int saved_stdout = -1;
    if (out_fd != -1) {
        fflush(stdout);
        saved_stdout = dup(STDOUT_FILENO);
        if (saved_stdout == -1 || dup2(out_fd, STDOUT_FILENO) == -1) {
            perror("mysh: dup");
            close_if_open(saved_stdout);
            close(out_fd);
            return 0;
        }
        close(out_fd);
    }

    int result = run_builtin(cmd);

    if (saved_stdout != -1) {
        fflush(stdout);
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }

    return result;
}

/* ---------- Running programs and pipelines ---------- */

// Per-stage info the parent sets up before forking.
typedef struct {
    char *path;   // program to execv, NULL for built-ins
    int in_fd;    // < file, or -1
    int out_fd;   // > or >> file, or -1
} stage_info;

static void cleanup_stages(stage_info *stages, int n) {
    for (int i = 0; i < n; i++) {
        free(stages[i].path);
        close_if_open(stages[i].in_fd);
        close_if_open(stages[i].out_fd);
    }
    free(stages);
}

static void close_pipes(int (*pipes)[2], int count) {
    for (int i = 0; i < count; i++) {
        close_if_open(pipes[i][0]);
        close_if_open(pipes[i][1]);
    }
}

// Runs in the child after fork(). Sets up stdin/stdout, closes every fd
// the child doesn't need, then execs (or runs the built-in). Never returns.
static void child_process(job *j, int idx, stage_info *stages, int (*pipes)[2],
                          int dev_null) {
    subcmd *cmd = &j->cmds[idx];
    stage_info *st = &stages[idx];
    int ok = 1;

    // stdin: < file, then the previous pipe, then /dev/null in batch mode
    if (st->in_fd != -1) {
        ok = dup2(st->in_fd, STDIN_FILENO) != -1;
    } else if (idx > 0) {
        ok = dup2(pipes[idx - 1][0], STDIN_FILENO) != -1;
    } else if (dev_null != -1) {
        ok = dup2(dev_null, STDIN_FILENO) != -1;
    }

    // stdout: > or >> file, then the next pipe, otherwise the shell's stdout
    if (ok && st->out_fd != -1) {
        ok = dup2(st->out_fd, STDOUT_FILENO) != -1;
    } else if (ok && idx < j->n - 1) {
        ok = dup2(pipes[idx][1], STDOUT_FILENO) != -1;
    }

    if (!ok) {
        perror("mysh: dup2");
        _exit(EXIT_FAILURE);
    }

    // Close every pipe end and file this child inherited. If a write end
    // stays open the next process never sees EOF and the pipeline hangs.
    close_pipes(pipes, j->n - 1);
    for (int i = 0; i < j->n; i++) {
        close_if_open(stages[i].in_fd);
        close_if_open(stages[i].out_fd);
    }
    close_if_open(dev_null);

    if (st->path == NULL) {
        // Built-in inside a pipeline. exit just exits the child here.
        _exit(run_builtin(cmd) ? EXIT_SUCCESS : EXIT_FAILURE);
    }

    execv(st->path, cmd->argv);
    fprintf(stderr, "mysh: %s: %s\n", cmd->argv[0], strerror(errno));
    _exit(EXIT_FAILURE);
}

static int run_processes(job *j, int interactive) {
    int n = j->n;
    int result = 0;
    int pipe_count = n - 1;
    int forked = 0;
    int dev_null = -1;

    stage_info *stages = calloc((size_t)n, sizeof(*stages));
    pid_t *pids = calloc((size_t)n, sizeof(*pids));
    int (*pipes)[2] = pipe_count > 0 ? calloc((size_t)pipe_count, sizeof(*pipes)) : NULL;

    if (stages == NULL || pids == NULL || (pipe_count > 0 && pipes == NULL)) {
        perror("mysh: calloc");
        free(stages);
        free(pids);
        free(pipes);
        return 0;
    }

    for (int i = 0; i < n; i++) {
        stages[i].in_fd = -1;
        stages[i].out_fd = -1;
    }
    for (int i = 0; i < pipe_count; i++) {
        pipes[i][0] = -1;
        pipes[i][1] = -1;
    }

    // Find every program and open every redirect file before forking, so
    // a bad command fails cleanly without starting half a pipeline.
    for (int i = 0; i < n; i++) {
        subcmd *cmd = &j->cmds[i];

        if (!is_builtin(cmd->argv[0])) {
            stages[i].path = find_program(cmd->argv[0]);
            if (stages[i].path == NULL) {
                goto done;
            }
        }

        if (!open_redirects(cmd, &stages[i].in_fd, &stages[i].out_fd)) {
            goto done;
        }
    }

    // Batch mode: children read /dev/null instead of the shell's input,
    // otherwise something like cat would eat the rest of the script.
    if (!interactive && stages[0].in_fd == -1) {
        dev_null = open("/dev/null", O_RDONLY);
        if (dev_null == -1) {
            perror("mysh: /dev/null");
            goto done;
        }
    }

    for (int i = 0; i < pipe_count; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("mysh: pipe");
            goto done;
        }
    }

    // Flush so buffered output doesn't get copied into the children
    fflush(stdout);
    fflush(stderr);

    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("mysh: fork");
            break;
        }
        if (pid == 0) {
            child_process(j, i, stages, pipes, dev_null);
        }
        pids[i] = pid;
        forked++;
    }

    // Parent has to close its copies of the pipe ends too, or the last
    // reader in the pipeline waits forever for EOF.
    close_pipes(pipes, pipe_count);
    for (int i = 0; i < pipe_count; i++) {
        pipes[i][0] = -1;
        pipes[i][1] = -1;
    }

    // Wait for every child, but only the last stage decides success
    for (int i = 0; i < forked; i++) {
        int status;
        while (waitpid(pids[i], &status, 0) == -1) {
            if (errno != EINTR) {
                perror("mysh: waitpid");
                status = -1;
                break;
            }
        }

        if (i == n - 1 && status != -1) {
            result = check_status(status, interactive);
        }
    }

    if (forked < n) {
        result = 0;
    }

done:
    close_pipes(pipes, pipe_count);
    close_if_open(dev_null);
    cleanup_stages(stages, n);
    free(pipes);
    free(pids);
    return result;
}

int run_job(job *j, int interactive) {
    if (j == NULL || j->n <= 0) {
        return 0;
    }

    // Parser should never give us an empty stage, but don't crash if it does
    for (int i = 0; i < j->n; i++) {
        if (j->cmds[i].argc <= 0 || j->cmds[i].argv == NULL) {
            fprintf(stderr, "mysh: syntax error\n");
            return 0;
        }
    }

    if (j->n == 1 && is_builtin(j->cmds[0].argv[0])) {
        return run_builtin_in_shell(&j->cmds[0]);
    }

    return run_processes(j, interactive);
}
