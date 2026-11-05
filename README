CS 214 Project III: mysh

Partners:
  John <LAST NAME>, <NETID>
  <PARTNER NAME>, <NETID>

Building and running
  make          builds mysh
  make test     builds and runs every test (unit tests and batch scripts)
  make clean

Files
  mysh.c       main(), input loop, prompt, interactive/batch mode
  parse.c/h    tokenizer, parser, wildcards (builds a job struct)
  job.h        the job/subcmd structs shared by the parser and executor
  exec.c       run_job(): program lookup, built-ins, redirection, pipes
  exec_test.c  unit tests for exec.c
  parser.c, tokenizer.c, wildcard*.c   unit tests for parse.c
  tests/run_tests.sh   runs the batch script tests in tests/*/
  tests/exec/          batch script tests for the execution side

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

We tested the executor two ways. exec_test.c builds job structs by hand
and calls run_job() directly, so it tests exec.c without depending on the
parser and can check return values (success/failure), which you can't see
from batch mode output. The scripts in tests/exec/ run through the real
mysh in batch mode, so they also check that the parser and executor work
together. tests/run_tests.sh runs each script in its own empty temp
directory, compares stdout to the .out file, checks the number of stderr
lines against the .err file for error cases, checks that mysh exits with
status 0, and kills mysh after 10 seconds so a hung pipeline shows up as a
failure instead of freezing the test run.

Things we thought were important to check, and how:

1. Program lookup and exit codes (exec_test.c: test_programs, tests/exec/
   not_found.sh, builtins.sh)
   - true succeeds, false fails, sh -c "exit 7" fails.
   - A bare name that isn't in any of the three directories prints
     "command not found" and fails.
   - Paths with a slash: /bin/true works, ./no_such_file fails, a file that
     exists but isn't executable fails. A copy of echo run as ./myecho works.

2. Redirection (test_redirection, redirect.sh, redirect_errors.sh)
   - > creates a file, >> appends, > on an existing file truncates it.
   - >> creates the file if it doesn't exist.
   - Created files have mode 640 (checked with stat).
   - < by itself, and < with > in both orders.
   - A missing input file or an output file in a directory that doesn't
     exist prints an error, fails the command, and the shell keeps going.
   - Built-ins with redirects: pwd > file writes the file and not the
     terminal, and stdout goes back to normal for the next command.

3. Pipes (test_pipes, pipes.sh, pipe_errors.sh)
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

4. Built-ins (test_builtins, builtins.sh)
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

5. exit (test_builtins, exit.sh, exit_args.sh, exit_pipe.sh)
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

7. Batch mode stdin (test_batch_stdin, batch_stdin.sh)
   In batch mode children should read /dev/null, not the shell's input.
   tests/exec/batch_stdin.sh is piped into mysh on stdin and contains a
   bare cat followed by more commands. If the child got the shell's stdin,
   cat would eat the rest of the script. In exec_test.c we put data on
   stdin, run cat in batch mode, and check that cat read nothing and the
   data is still there for the shell. We checked that both tests fail if
   the /dev/null code is removed.

8. Memory and file descriptors
   We ran mysh on the combined test scripts and ran exec_test under
   valgrind --leak-check=full --track-fds=yes. No leaks, no invalid reads
   or writes, and no extra file descriptors left open in the shell after
   running pipelines and redirects.

Test plan: parsing side (mysh.c, parse.c)

<PARTNER: add your section here>
