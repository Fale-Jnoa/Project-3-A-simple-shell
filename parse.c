
#include "parse.h"

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
