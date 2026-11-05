
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
