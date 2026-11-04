#ifndef PARSE_H
#define PARSE_H
#include <stddef.h>
#include "job.h"
typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} token_list;
int tokenize_line(const char *line, token_list *out);
void free_tokens(token_list *tokens);
int wildcard_match_name(const char *pattern, const char *name);
int collect_wildcard_matches(const char *pattern, char ***out, size_t *count);
void free_wildcard_matches(char **matches, size_t count);
int parse_job(const char *line, job *out);
void free_job(job *j);
#endif
