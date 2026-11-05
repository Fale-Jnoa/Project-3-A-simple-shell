CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -g

PARSE_TESTS = parser_test tokenizer_test wildcard_test wildcard_dir_test wildcard_job_test
TESTS = exec_test $(PARSE_TESTS)

all: mysh

mysh: mysh.o parse.o exec.o
	$(CC) $(CFLAGS) -o $@ $^

mysh.o: mysh.c parse.h job.h
parse.o: parse.c parse.h job.h
exec.o: exec.c job.h

# execution tests (hardcoded jobs, no parser)
exec_test: exec_test.o exec.o
	$(CC) $(CFLAGS) -o $@ $^

exec_test.o: exec_test.c job.h

# parser tests
parser_test: parser.c parse.o
	$(CC) $(CFLAGS) -o $@ $^
tokenizer_test: tokenizer.c parse.o
	$(CC) $(CFLAGS) -o $@ $^
wildcard_test: wildcard.c parse.o
	$(CC) $(CFLAGS) -o $@ $^
wildcard_dir_test: wildcard_dir.c parse.o
	$(CC) $(CFLAGS) -o $@ $^
wildcard_job_test: wildcard_job.c parse.o
	$(CC) $(CFLAGS) -o $@ $^

test: mysh $(TESTS)
	@for t in $(TESTS); do echo "== $$t"; ./$$t || exit 1; done
	@echo "== batch script tests"
	@sh tests/run_tests.sh

clean:
	rm -f mysh *.o $(TESTS)

.PHONY: all test clean
