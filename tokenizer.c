
#include "parse.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void check(const char *line,
                  const char *expected[],
                  size_t expected_count) {
    token_list tokens;

    assert(tokenize_line(line, &tokens) == 0);
    assert(tokens.count == expected_count);

    for (size_t i = 0; i < expected_count; i++) {
        assert(strcmp(tokens.items[i], expected[i]) == 0);
    }

    if (tokens.items) {
        assert(tokens.items[tokens.count] == NULL);
    }

    free_tokens(&tokens);
}

int main(void) {
    check("echo hello",
          (const char *[]){"echo", "hello"}, 2);

    check("cat < in.txt >> out.txt",
          (const char *[]){"cat", "<", "in.txt", ">>", "out.txt"}, 5);

    check("echo hi|wc -c",
          (const char *[]){"echo", "hi", "|", "wc", "-c"}, 5);

    check("echo hi # comment > ignored",
          (const char *[]){"echo", "hi"}, 2);

    check("echo a#ignored",
          (const char *[]){"echo", "a"}, 2);

    check("  # comment only", NULL, 0);

    check(" \t ", NULL, 0);

    check("echo *.txt",
          (const char *[]){"echo", "*.txt"}, 2);

    puts("Tokenizer: 8/8 tests passed");
    return 0;
}
