
#include "parse.h"
#include <limits.h>
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void free_tokens(token_list *tokens) {
    for (size_t i = 0; i < tokens->count; i++) {
        free(tokens->items[i]);
    }

    free(tokens->items);
    tokens->items = NULL;
    tokens->count = 0;
    tokens->capacity = 0;
}

static int append_token(token_list *tokens,
                        const char *start,
                        size_t length) {
    if (tokens->count + 1 >= tokens->capacity) {
        size_t new_capacity =
            tokens->capacity ? tokens->capacity * 2 : 8;

        if (new_capacity < tokens->capacity ||
            new_capacity > SIZE_MAX / sizeof(*tokens->items)) {
            return -1;
        }

        char **new_items = realloc(
            tokens->items,
            new_capacity * sizeof(*tokens->items)
        );

        if (!new_items) {
            return -1;
        }

        tokens->items = new_items;
        tokens->capacity = new_capacity;
    }

    char *token = malloc(length + 1);

    if (!token) {
        return -1;
    }

    memcpy(token, start, length);
    token[length] = '\0';

    tokens->items[tokens->count++] = token;
    tokens->items[tokens->count] = NULL;

    return 0;
}

int tokenize_line(const char *line, token_list *out) {
    *out = (token_list){0};

    const char *p = line;

    while (*p) {
        if (isspace((unsigned char)*p)) {
            p++;
            continue;
        }

        if (*p == '#') {
            break;
        }

        const char *start = p;

        if (*p == '>' && p[1] == '>') {
            p += 2;
        } else if (*p == '<' || *p == '>' || *p == '|') {
            p++;
        } else {
            while (*p &&
                   !isspace((unsigned char)*p) &&
                   *p != '#' &&
                   *p != '<' &&
                   *p != '>' &&
                   *p != '|') {
                p++;
            }
        }

        if (append_token(out, start,
                         (size_t)(p - start)) == -1) {
            free_tokens(out);
            return -1;
        }
    }

    return 0;
}


static int is_redirect(const char *s) {
    return strcmp(s, "<") == 0 ||
           strcmp(s, ">") == 0 ||
           strcmp(s, ">>") == 0;
}

static int is_special(const char *s) {
    return is_redirect(s) || strcmp(s, "|") == 0;
}

static char *copy_token(const char *s) {
    size_t len = strlen(s);
    char *copy = malloc(len + 1);

    if (copy != NULL) {
        memcpy(copy, s, len + 1);
    }

    return copy;
}

void free_job(job *j) {
    if (j == NULL) {
        return;
    }

    for (int i = 0; i < j->n; i++) {
        subcmd *cmd = &j->cmds[i];

        for (int k = 0; k < cmd->argc; k++) {
            free(cmd->argv[k]);
        }

        free(cmd->argv);
        free(cmd->infile);
        free(cmd->outfile);
    }

    free(j->cmds);
    j->cmds = NULL;
    j->n = 0;
}

static int parse_stage(const token_list *tokens,
                       size_t start,
                       size_t end,
                       subcmd *cmd) {
    if (start == end) {
        return -1;
    }

    size_t slots = end - start + 1;

    if (slots > SIZE_MAX / sizeof(*cmd->argv)) {
        return -2;
    }

    cmd->argv = calloc(slots, sizeof(*cmd->argv));

    if (cmd->argv == NULL) {
        return -2;
    }

    for (size_t i = start; i < end; i++) {
        const char *token = tokens->items[i];

        if (is_redirect(token)) {
            if (i + 1 >= end ||
                is_special(tokens->items[i + 1])) {
                return -1;
            }

            const char *filename = tokens->items[++i];

            if (strcmp(token, "<") == 0) {
                if (cmd->infile != NULL) {
                    return -1;
                }

                cmd->infile = copy_token(filename);
                if (!cmd->infile) {
                    return -2;
                }
            } else {
                if (cmd->outfile != NULL) {
                    return -1;
                }

                cmd->outfile = copy_token(filename);
                if (!cmd->outfile) {
                    return -2;
                }

                cmd->append =
                    strcmp(token, ">>") == 0;
            }
        } else {
            char *arg = copy_token(token);

            if (arg == NULL) {
                return -2;
            }

            cmd->argv[cmd->argc++] = arg;
        }
    }

    // A stage must contain a command name.
    return cmd->argc > 0 ? 0 : -1;
}

int parse_job(const char *line, job *out) {
    *out = (job){0};

    token_list tokens;

    if (tokenize_line(line, &tokens) != 0) {
        return -2;
    }

    if (tokens.count == 0) {
        free_tokens(&tokens);
        return 0;
    }

    size_t stages = 1;

    for (size_t i = 0; i < tokens.count; i++) {
        if (strcmp(tokens.items[i], "|") == 0) {
            stages++;
        }
    }

    if (stages > INT_MAX ||
        stages > SIZE_MAX / sizeof(*out->cmds) ||
        tokens.count > INT_MAX) {
        free_tokens(&tokens);
        return -2;
    }

    out->cmds = calloc(stages, sizeof(*out->cmds));

    if (out->cmds == NULL) {
        free_tokens(&tokens);
        return -2;
    }

    out->n = (int)stages;

    size_t start = 0;
    size_t stage_index = 0;
    int result = 1;

    for (size_t i = 0; i <= tokens.count; i++) {
        if (i == tokens.count ||
            strcmp(tokens.items[i], "|") == 0) {

            int status = parse_stage(
                &tokens, start, i,
                &out->cmds[stage_index]
            );

            if (status != 0) {
                result = status;
                break;
            }

            stage_index++;
            start = i + 1;
        }
    }

    free_tokens(&tokens);

    if (result != 1) {
        free_job(out);
    }

    return result;
}
