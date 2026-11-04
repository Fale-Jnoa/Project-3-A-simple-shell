
#ifndef PARSE_H
#define PARSE_H

#include <stddef.h>

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} token_list;

int tokenize_line(const char *line, token_list *out);
void free_tokens(token_list *tokens);

#endif
