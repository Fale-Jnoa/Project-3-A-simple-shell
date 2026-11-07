#include "parse.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ---------- Tokenization ---------- */

void free_tokens(token_list *tokens) {
    if (tokens == NULL) {
        return;
    }

    for (size_t i = 0; i < tokens->count; i++) {
        free(tokens->items[i]);
    }

    free(tokens->items);
    tokens->items = NULL;
    tokens->count = 0;
    tokens->capacity = 0;
}

static int append_token(token_list *tokens, const char *start, size_t length) {
    /* Reserve an extra slot for the NULL terminator. */
    if (tokens->count + 1 >= tokens->capacity) {
        size_t new_capacity = tokens->capacity ? tokens->capacity * 2 : 8;

        if (new_capacity < tokens->capacity ||
            new_capacity > SIZE_MAX / sizeof(*tokens->items)) {
            return -1;
        }

        char **new_items = realloc(tokens->items,
                                   new_capacity * sizeof(*tokens->items));
        if (new_items == NULL) {
            return -1;
        }

        tokens->items = new_items;
        tokens->capacity = new_capacity;
    }

    if (length == SIZE_MAX) {
        return -1;
    }

    char *copy = malloc(length + 1);
    if (copy == NULL) {
        return -1;
    }

    memcpy(copy, start, length);
    copy[length] = '\0';

    tokens->items[tokens->count++] = copy;
    tokens->items[tokens->count] = NULL;
    return 0;
}

int tokenize_line(const char *line, token_list *out) {
    *out = (token_list){0};
    const char *p = line;

    while (*p != '\0') {
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
            while (*p != '\0' &&
                   !isspace((unsigned char)*p) &&
                   *p != '#' && *p != '<' &&
                   *p != '>' && *p != '|') {
                p++;
            }
        }

        if (append_token(out, start, (size_t)(p - start)) != 0) {
            free_tokens(out);
            return -1;
        }
    }

    return 0;
}

/* ---------- Wildcard matching and directory scanning ---------- */

int wildcard_match_name(const char *pattern, const char *name) {
    const char *star = strchr(pattern, '*');

    if (star == NULL) {
        return strcmp(pattern, name) == 0;
    }

    /* The assignment permits one asterisk per filename pattern. */
    if (strchr(star + 1, '*') != NULL) {
        return 0;
    }

    if (pattern[0] == '*' && name[0] == '.') {
        return 0;
    }

    size_t prefix_len = (size_t)(star - pattern);
    size_t suffix_len = strlen(star + 1);
    size_t name_len = strlen(name);

    if (name_len < prefix_len || name_len - prefix_len < suffix_len) {
        return 0;
    }

    if (strncmp(pattern, name, prefix_len) != 0) {
        return 0;
    }

    return strcmp(star + 1, name + name_len - suffix_len) == 0;
}

void free_wildcard_matches(char **matches, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(matches[i]);
    }
    free(matches);
}

static int compare_matches(const void *a, const void *b) {
    const char *const *left = a;
    const char *const *right = b;
    return strcmp(*left, *right);
}

int collect_wildcard_matches(const char *pattern, char ***out, size_t *count) {
    *out = NULL;
    *count = 0;

    const char *star = strchr(pattern, '*');
    if (star == NULL || strchr(star + 1, '*') != NULL) {
        return 0;
    }

    const char *slash = strrchr(pattern, '/');
    const char *name_pattern = slash == NULL ? pattern : slash + 1;

    /* Only an asterisk in the final pathname component is supported. */
    if (star < name_pattern) {
        return 0;
    }

    size_t prefix_len = slash == NULL ? 0 : (size_t)(slash - pattern + 1);
    char *prefix = malloc(prefix_len + 1);
    if (prefix == NULL) {
        return -1;
    }

    memcpy(prefix, pattern, prefix_len);
    prefix[prefix_len] = '\0';

    DIR *dir = opendir(prefix_len == 0 ? "." : prefix);
    if (dir == NULL) {
        free(prefix);
        /* No accessible matching directory: leave the token unchanged. */
        return 0;
    }

    char **results = NULL;
    size_t used = 0;
    size_t capacity = 0;
    int failed = 0;

    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(dir);
        if (entry == NULL) {
            if (errno != 0) {
                failed = 1;
            }
            break;
        }

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0 ||
            !wildcard_match_name(name_pattern, entry->d_name)) {
            continue;
        }

        size_t name_len = strlen(entry->d_name);
        if (prefix_len > SIZE_MAX - name_len - 1) {
            failed = 1;
            break;
        }

        char *path = malloc(prefix_len + name_len + 1);
        if (path == NULL) {
            failed = 1;
            break;
        }

        memcpy(path, prefix, prefix_len);
        memcpy(path + prefix_len, entry->d_name, name_len + 1);

        if (used == capacity) {
            size_t new_capacity = capacity ? capacity * 2 : 8;
            if (new_capacity < capacity ||
                new_capacity > SIZE_MAX / sizeof(*results)) {
                free(path);
                failed = 1;
                break;
            }

            char **temp = realloc(results,
                                  new_capacity * sizeof(*results));
            if (temp == NULL) {
                free(path);
                failed = 1;
                break;
            }

            results = temp;
            capacity = new_capacity;
        }

        results[used++] = path;
    }

    if (closedir(dir) != 0) {
        failed = 1;
    }
    free(prefix);

    if (failed) {
        free_wildcard_matches(results, used);
        return -1;
    }

    if (used > 1) {
        qsort(results, used, sizeof(*results), compare_matches);
    }

    *out = results;
    *count = used;
    return 0;
}

/* ---------- Job parsing and cleanup ---------- */

static int is_redirect(const char *s) {
    return strcmp(s, "<") == 0 ||
           strcmp(s, ">") == 0 ||
           strcmp(s, ">>") == 0;
}

static int is_special(const char *s) {
    return is_redirect(s) || strcmp(s, "|") == 0;
}

static char *copy_token(const char *s) {
    size_t length = strlen(s);
    char *copy = malloc(length + 1);
    if (copy != NULL) {
        memcpy(copy, s, length + 1);
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

/* Takes ownership of arg on success only. */
static int append_arg(subcmd *cmd, size_t *capacity, char *arg) {
    if (cmd->argc == INT_MAX) {
        return -2;
    }

    size_t needed = (size_t)cmd->argc + 2;
    if (needed > *capacity) {
        size_t new_capacity = *capacity ? *capacity : 8;

        while (new_capacity < needed) {
            if (new_capacity > SIZE_MAX / 2) {
                return -2;
            }
            new_capacity *= 2;
        }

        if (new_capacity > SIZE_MAX / sizeof(*cmd->argv)) {
            return -2;
        }

        char **temp = realloc(cmd->argv,
                              new_capacity * sizeof(*cmd->argv));
        if (temp == NULL) {
            return -2;
        }

        cmd->argv = temp;
        *capacity = new_capacity;
    }

    cmd->argv[cmd->argc++] = arg;
    cmd->argv[cmd->argc] = NULL;
    return 0;
}

static int parse_stage(const token_list *tokens, size_t start,
                       size_t end, subcmd *cmd) {
    if (start == end) {
        return -1;
    }

    size_t capacity = 0;

    for (size_t i = start; i < end; i++) {
        const char *token = tokens->items[i];

        if (is_redirect(token)) {
            if (i + 1 >= end || is_special(tokens->items[i + 1])) {
                return -1;
            }

            const char *filename = tokens->items[++i];
            if (strcmp(token, "<") == 0) {
                if (cmd->infile != NULL) {
                    return -1;
                }

                cmd->infile = copy_token(filename);
                if (cmd->infile == NULL) {
                    return -2;
                }
            } else {
                if (cmd->outfile != NULL) {
                    return -1;
                }

                cmd->outfile = copy_token(filename);
                if (cmd->outfile == NULL) {
                    return -2;
                }
                cmd->append = strcmp(token, ">>") == 0;
            }
            continue;
        }

        if (strchr(token, '*') != NULL) {
            char **matches = NULL;
            size_t match_count = 0;

            if (collect_wildcard_matches(token, &matches, &match_count) != 0) {
                return -2;
            }

            if (match_count > 0) {
                for (size_t k = 0; k < match_count; k++) {
                    char *arg = matches[k];
                    matches[k] = NULL;

                    if (append_arg(cmd, &capacity, arg) != 0) {
                        free(arg);
                        free_wildcard_matches(matches, match_count);
                        return -2;
                    }
                }

                free_wildcard_matches(matches, match_count);
                continue;
            }

            free_wildcard_matches(matches, match_count);
        }

        // Keep unmatched wildcard patterns unchanged.
        char *arg = copy_token(token);
        if (arg == NULL) {
            return -2;
        }

        if (append_arg(cmd, &capacity, arg) != 0) {
            free(arg);
            return -2;
        }
    }

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

    if (stages > INT_MAX || tokens.count > INT_MAX ||
        stages > SIZE_MAX / sizeof(*out->cmds)) {
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
        if (i == tokens.count || strcmp(tokens.items[i], "|") == 0) {
            int status = parse_stage(&tokens, start, i,
                                     &out->cmds[stage_index]);
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
