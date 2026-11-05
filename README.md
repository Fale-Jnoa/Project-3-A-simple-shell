CS 214 Project III: mysh

Partners:
  John Inoa, jci30
  Rushil Pandya, rp1478

Building and running
  make          builds mysh
  make clean

Files
  mysh.c       main(), input loop, prompt, interactive/batch mode
  parse.c/h    tokenizer, parser, wildcards (builds a job struct)
  job.h        the job/subcmd structs shared by the parser and executor
  exec.c       run_job(): program lookup, built-ins, redirection, pipes
  tests/exec_test.c  unit tests for exec.c
  tests/parser.c, tests/tokenizer.c, tests/wildcard*.c   parser and wildcard unit tests

Design notes (execution)
  The parser hands run_job() a job, which is an array of subcmds (one per
  pipeline stage). run_job() returns 1 if the command succeeded and 0 if it
  failed.

  - A single built-in with no pipe runs inside the shell process, so cd
    actually changes mysh's directory. If it has a > or >> redirect we dup
    stdout, point it at the file, run the built-in, then put stdout back.
  - Everything else is forked. Before forking anything, we look up every
    program and open every redirect file. If any of that fails, we print
    an error and fail the command without starting any processes, so a
    bad command never leaves half a pipeline running.
  - Program lookup: names with a / are used as-is (must pass access X_OK).
    Other names are searched in /usr/local/bin, /usr/bin, /bin in that
    order using access(). We use execv(), not execvp().
  - A pipeline of n stages uses n-1 pipes. Each child dup2()s its stdin
    and stdout, then closes every pipe end and redirect file it inherited.
    The parent closes all of its pipe ends after forking. If any write end
    stays open the reader never gets EOF, so this matters a lot.
  - stdin priority for a child: < file, then the previous pipe, then
    /dev/null in batch mode (in interactive mode it inherits the terminal).
    stdout priority: > or >> file, then the next pipe, then the shell's
    stdout.
  - We wait for every child, but only the last stage decides whether the
    command succeeded. In interactive mode, if the last stage exits with a
    nonzero code we print "Exited with status N", and if it was killed we
    print "Terminated by signal X" using strsignal().
  - Built-ins in a pipeline run in the child (so cd in a pipeline doesn't
    change the shell's directory). exit in a pipeline just exits the child;
    mysh.c notices exit in any stage and stops after the command finishes.
  - Files made by > and >> use mode 0640. > uses O_TRUNC, >> uses O_APPEND.
  - which does not special-case built-ins, it only does the directory
    search (so "which pwd" prints /usr/bin/pwd). Names with a / are printed
    as-is if executable. It fails with no arguments or if any name is not
    found. pwd fails if given arguments, cd fails with more than one.

Test plan: execution side (exec.c)

We tested the executor independently using tests/exec_test.c,
which builds job structures and calls run_job() directly.
We also tested the complete mysh executable manually with
batch commands, interactive commands, redirection, pipelines,
wildcards, and error handling. The executor passed 74 unit
checks, including a sanitizer-enabled run.

Things we thought were important to check, and how:

1. Program lookup and exit codes (exec_test.c: test_programs)
   - true succeeds, false fails, sh -c "exit 7" fails.
   - A bare name that isn't in any of the three directories prints
     "command not found" and fails.
   - Paths with a slash: /bin/true works, ./no_such_file fails, a file that
     exists but isn't executable fails. A copy of echo run as ./myecho works.

2. Redirection (exec_test.c: test_redirection)
   - > creates a file, >> appends, > on an existing file truncates it.
   - >> creates the file if it doesn't exist.
   - Created files have mode 640 (checked with stat).
   - < by itself, and < with > in both orders.
   - A missing input file or an output file in a directory that doesn't
     exist prints an error, fails the command, and the shell keeps going.
   - Built-ins with redirects: pwd > file writes the file and not the
     terminal, and stdout goes back to normal for the next command.

3. Pipes (exec_test.c: test_pipes)
   - echo hello | cat, echo one two three | wc -w.
   - Three and four stage pipelines (ls | sort -r | head -n 2 and
     echo | cat | cat | cat).
   - seq 1 200000 | cat | wc -l, which moves enough data to fill a pipe
     buffer. We also checked this test catches bugs by temporarily removing
     the parent's close() calls on the pipes: the test hung and timed out.
   - Success only depends on the last stage: false | true succeeds and
     true | false fails.
   - An unknown program anywhere in the pipeline fails the command without
     starting the other stages.

4. Built-ins (exec_test.c: test_builtins)
   - cd into a subdirectory and back with cd .., checked with pwd.
   - cd with no argument goes to HOME (the test script sets HOME).
   - cd to a missing directory and cd with two arguments fail and leave the
     directory unchanged. pwd with an argument fails.
   - cd in a pipeline doesn't change the shell's directory.
   - pwd as the first stage of a pipeline.
   - which with no args fails, which true prints a path from one of the
     three directories, which on a missing name prints nothing and fails,
     which with one good and one bad name prints the good one and fails,
     which /bin/sh prints /bin/sh, and which pwd finds the real program.

5. exit (exec_test.c: test_builtins)
   - exit stops the shell, nothing after it runs, and mysh exits with 0.
   - exit with arguments ignores them.
   - exit | echo piped: the echo still runs, then the shell exits.

6. Interactive status messages (test_status_messages)
   These tests point the shell's stdout at a file while run_job() runs.
   - A program exiting with 3 prints "Exited with status 3".
   - A program killed with signal 9 prints "Terminated by signal ...".
   - A successful program prints nothing.
   - In batch mode nothing is printed even when the command fails.
   - In a pipeline the message is for the last stage (false | sh -c "exit 4"
     prints status 4).
   We also ran mysh by hand in a terminal to check the messages show up
   before the next prompt.

7. Batch mode stdin (exec_test.c and manual integration tests)
   In batch mode children should read /dev/null, not the shell's input.
   We ran cat followed by echo and exit through mysh in batch
   mode. The later commands still executed, demonstrating that
   cat did not consume the shell's remaining input.

8. Memory safety
   We compiled both the executor tests and complete mysh shell
   using AddressSanitizer and UndefinedBehaviorSanitizer.
   All 74 executor checks passed, and the combined-shell
   integration test completed without sanitizer reports.

Test plan: parsing side (mysh.c, parse.c)

## Shell Input and Parser Testing

### Testing Strategy
The input handling, tokenizer, parser, and wildcard expansion
were tested independently before integration. Each component
was compiled with GCC using -Wall, -Wextra, -Werror, and
-pedantic. AddressSanitizer and UndefinedBehaviorSanitizer
were used for additional memory-safety checks.

A temporary execution stub was used to verify parsed job
structures without depending on the actual executor.

### Input Handling
- Verified interactive and batch mode detection.
- Tested valid and missing batch input files.
- Tested multiple commands and empty lines.
- Tested long input lines and EOF without a trailing newline.
- Verified interactive prompts and welcome/goodbye messages.

### Tokenizer
- Tested whitespace and ordinary arguments.
- Tested <, >, >>, and | operators.
- Tested comments and comment-only lines.
- Verified all 8 tokenizer unit tests passed.

### Job Parser
- Tested simple commands and multiple pipeline stages.
- Tested input, output, and append redirection.
- Verified argument arrays are NULL-terminated.
- Tested invalid redirection and empty pipeline stages.
- Verified that syntax errors do not stop subsequent commands.

### Wildcards
- Verified 15 filename-matching tests.
- Tested prefix, suffix, and combined wildcard patterns.
- Tested hidden-file matching behavior.
- Tested directory scanning and unmatched patterns.
- Verified wildcard expansion inside parsed jobs.

### Integration Testing
- Tested shell input through the parser and execution stub.
- Verified exit stops subsequent commands.
- Verified exit inside a pipeline.
- Tested comments, malformed commands, and error recovery.
- Tested batch file execution and EOF handling.

### Memory Safety
Parser and shell integration tests passed with
AddressSanitizer and/or UndefinedBehaviorSanitizer enabled.

### Full-Shell Verification
After integration with exec.c, tested actual command execution,
built-ins, redirection, pipelines, wildcard expansion, batch
stdin isolation, and interactive error and signal reporting.

The executor passed all 74 unit checks, including sanitizer-
enabled testing. The complete shell also passed integration
tests with AddressSanitizer and UndefinedBehaviorSanitizer.
