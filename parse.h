
#ifndef PARSE_H
#define PARSE_H

#include <stddef.h>
#include "job.h"

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} token_list;

// Tokenizer
int tokenize_line(const char *line, token_list *out);
void free_tokens(token_list *tokens);

// Job parser
//  1 = valid job
//  0 = empty command
// -1 = syntax error
// -2 = allocation failure
int parse_job(const char *line, job *out);

// Memory cleanup
void free_job(job *j);

// Returns 1 if the filename matches the wildcard pattern.
int wildcard_match_name(const char *pattern, const char *name);

#endif
