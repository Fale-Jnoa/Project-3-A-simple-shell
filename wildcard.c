
#include "parse.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    // Prefix matching
    assert(wildcard_match_name("foo*", "foobar") == 1);
    assert(wildcard_match_name("foo*", "foo") == 1);
    assert(wildcard_match_name("foo*", "barfoo") == 0);

    // Prefix and suffix matching
    assert(wildcard_match_name("foo*bar", "foo123bar") == 1);
    assert(wildcard_match_name("foo*bar", "foobar") == 1);
    assert(wildcard_match_name("foo*bar", "foo123baz") == 0);

    // Suffix matching
    assert(wildcard_match_name("*.txt", "report.txt") == 1);
    assert(wildcard_match_name("*.txt", "report.c") == 0);

    // Hidden files
    assert(wildcard_match_name("*.txt", ".secret.txt") == 0);
    assert(wildcard_match_name(".*", ".hidden") == 1);

    // Asterisk alone
    assert(wildcard_match_name("*", "hello") == 1);
    assert(wildcard_match_name("*", ".hidden") == 0);

    // Exact match without a wildcard
    assert(wildcard_match_name("hello", "hello") == 1);
    assert(wildcard_match_name("hello", "world") == 0);

    // Prefix and suffix must not overlap.
    assert(wildcard_match_name("ab*bc", "abc") == 0);

    puts("Wildcard matching: 15/15 tests passed");
    return 0;
}
