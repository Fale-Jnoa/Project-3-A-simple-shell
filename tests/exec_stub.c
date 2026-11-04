
#include "job.h"
#include <stdio.h>

int run_job(job *j, int interactive) {
    (void)interactive;

    for (int i = 0; i < j->n; i++) {
        subcmd *cmd = &j->cmds[i];

        printf("STAGE %d:", i + 1);

        for (int k = 0; k < cmd->argc; k++) {
            printf(" [%s]", cmd->argv[k]);
        }

        if (cmd->infile != NULL) {
            printf(" < [%s]", cmd->infile);
        }

        if (cmd->outfile != NULL) {
            printf(" %s [%s]",
                   cmd->append ? ">>" : ">",
                   cmd->outfile);
        }

        putchar('\n');
    }

    return 0;
}
