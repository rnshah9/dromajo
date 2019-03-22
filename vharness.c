//******************************************************************************
// Copyright (C) 2018,2019, Esperanto Technologies Inc.
// The copyright to the computer program(s) herein is the
// property of Esperanto Technologies, Inc. All Rights Reserved.
// The program(s) may be used and/or copied only with
// the written permission of Esperanto Technologies and
// in accordance with the terms and conditions stipulated in the
// agreement/contract under which the program(s) have been supplied.
//------------------------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/stat.h>
#include <signal.h>

#include "cutils.h"
#include "iomem.h"
#include "virtio.h"
#include "riscv_cpu.h"

//#define REGRESS_RISCV_COSIM 1
#ifdef REGRESS_RISCV_COSIM
#include "riscvemu_cosim.h"
#endif


int iterate_core(VirtMachine *m)
{
    if (m->maxinsns-- <= 0)
        /* Succeed after N instructions without failure. */
        return 0;

    /* Instruction that raises exceptions should be marked as such in
     * the trace of retired instructions.  Breaking this caused
     * ARCHSIM-74.
     */
    RISCVMachine  *rvm  = (RISCVMachine *)m;
    RISCVCPUState *cpu  = rvm->cpu_state;
    uint64_t last_pc    = virt_machine_get_pc(m);
    int      priv       = virt_machine_get_priv_level(m);
    uint32_t insn_raw   = -1;
    (void) riscv_read_insn(cpu, &insn_raw, last_pc);
    int      keep_going = virt_machine_run(m);

    if (last_pc == virt_machine_get_pc(m))
        return 0;

    if (m->trace) {
        --m->trace;
        return keep_going;
    }

    fprintf(stderr, "%d 0x%016"PRIx64" (0x%08x)", priv, last_pc,
            (insn_raw & 3) == 3 ? insn_raw : (uint16_t) insn_raw);

    uint64_t dummy1, dummy2;
    int iregno = virt_machine_get_most_recently_written_reg(m, &dummy1);
    int fregno = virt_machine_get_most_recently_written_fp_reg(m, &dummy2);

    if (cpu->pending_exception != -1)
        fprintf(stderr, " exception %d, tval %016lx", cpu->pending_exception,
                virt_machine_get_priv_level(m) == PRV_M ? cpu->mtval : cpu->stval);
    else if (iregno > 0)
        fprintf(stderr, " x%2d 0x%016"PRIx64, iregno, virt_machine_get_reg(m, iregno));
    else if (fregno >= 0)
        fprintf(stderr, " f%2d 0x%016"PRIx64, fregno, virt_machine_get_fpreg(m, fregno));

    putc('\n', stderr);

    return keep_going;
}

int main(int argc, char **argv)
{
#ifdef REGRESS_RISCV_COSIM
    riscvemu_cosim_state_t *costate = 0;
    costate = riscvemu_cosim_init(argc, argv);

    if (!costate)
        return 1;

    do ; while (!riscvemu_cosim_step(costate, 0, 0, 0, 0, false));
#else
    VirtMachine *m = virt_machine_main(argc, argv);

    if (!m)
        return 1;

    int keep_going;
    do {
        keep_going = iterate_core(m);
    } while (keep_going);

    int benchmark_exit_code = virt_machine_benchmark_exit_code(m);
    if (benchmark_exit_code != 0){
        fprintf(stderr, "\nBenchmark exited with code: %i \n", benchmark_exit_code);
        return 1;
    }

    fprintf(stderr,"\nPower off.\n");

    virt_machine_end(m);
#endif

    return 0;
}
