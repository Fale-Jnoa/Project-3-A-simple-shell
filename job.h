#ifndef JOB_H
#define JOB_H

typedef struct {
    char **argv;      // NULL-terminated, wildcards already expanded
    int argc;
    char *infile;     // NULL if no < redirect
    char *outfile;    // NULL if no > or >>
    int append;       // 1 if >>, 0 if >
} subcmd;

typedef struct {
    subcmd *cmds;     // one per pipeline stage
    int n;            // number of stages (pipes = n - 1)
} job;

// Back end implements this. Returns 1 on success, 0 on failure.
int run_job(job *j, int interactive);

#endif
