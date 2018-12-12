//******************************************************************************
// Copyright (C) 2018, Esperanto Technologies Inc.
// The copyright to the computer program(s) herein is the
// property of Esperanto Technologies, Inc. All Rights Reserved.
// The program(s) may be used and/or copied only with
// the written permission of Esperanto Technologies and
// in accordance with the terms and conditions stipulated in the
// agreement/contract under which the program(s) have been supplied.
//------------------------------------------------------------------------------

/*
 * Test bench for riscvemu_cosim API
 *
 * Copyright (c) 2018 Esperanto Technologies
 *
 * Parse the trace output and check that we cosim correctly.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "riscvemu_cosim.h"

void usage(char *progname)
{
    fprintf(stderr,
            "Usage:\n"
            "  %s cosim $trace $riscvemuargs ...\n"
            "  %s read $trace\n",
            progname, progname);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    char *progname = argv[0];
    bool cosim = false;
    if (argc < 3)
        usage(progname);

    char *cmd = argv[1];
    if (strcmp(cmd, "read") == 0)
        cosim = false;
    else if (strcmp(cmd, "cosim") == 0)
        cosim = true;
    else
        usage(progname);

    char *trace_name = argv[2];
    FILE *f = fopen(trace_name, "r");
    if (!f) {
        perror(trace_name);
        usage(progname);
    }

    riscvemu_cosim_state_t *s = NULL;
    if (cosim) {
        /* Prep args for riscvemu_cosim_init */
        argc -= 2;
        argv += 2;
        argv[0] = progname;

        s = riscvemu_cosim_init(argc, argv);
        if (!s)
            usage(progname);
    }

    for (int lineno = 1; !feof(f); ++lineno) {
        char buf[99];
        uint64_t insn_addr, wdata;
        uint32_t insn, rd;
        int priv;

        if (!fgets(buf, sizeof buf, f))
            break;

        int got = sscanf(buf, "%d 0x%lx (0x%x) x%d 0x%lx", &priv, &insn_addr,
                         &insn, &rd, &wdata);

        switch (got) {
        case 3:
            printf("%d %016lx %08x                           DASM(%08x)\n",
                   priv, insn_addr, insn, insn);
            break;
        case 5: printf("%d %016lx %08x [x%-2d <- %016lx] DASM(%08x)\n",
                       priv, insn_addr, insn, rd, wdata, insn);
            break;

        default:
            fprintf(stderr, "%s:%d: couldn't parse (%d)\n",
                    trace_name, lineno, got);
            /* fall thru */
        case 0:
        case -1:
            continue;
        }

        if (cosim) {
            int r = riscvemu_cosim_step(s, insn_addr, insn, wdata,
                                        0, 0, 0, true);
            if (r) {
                printf("Exited with %08x\n", r);
                break;
            }
        }
    }

    free(s);

    printf("\nSUCCESS, PASSED, GOOD!\n");

    exit(EXIT_SUCCESS);
}
