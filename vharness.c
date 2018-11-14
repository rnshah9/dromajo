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
    uint64_t instret = virt_machine_get_instret(m);
    int keep_going, priv;
    uint64_t last_pc, prev_instret;
    uint32_t insn_raw = 0;

    if (m->maxinsns-- <= 0)
        /* Succeed after N instructions without failure. */
        return 0;

    /* Loop until an instruction retires.  This is important because
     * exceptions, such as illegal instruction must not be included in
     * the trace of retired instructions.  Breaking this caused
     * ARCHSIM-74.
     */
    do {
        prev_instret = instret; last_pc = virt_machine_get_pc(m);
        priv = virt_machine_get_priv_level(m);
        virt_machine_read_insn(m, &insn_raw, last_pc);

        keep_going = virt_machine_run(m);

        instret = virt_machine_get_instret(m);
    } while (keep_going && prev_instret == instret);

    if (last_pc == virt_machine_get_pc(m))
        return 0;

    if (instret < m->trace && instret < m->maxinsns)
        return keep_going;

    if (instret >= m->trace) {
        if ((insn_raw & 3) == 3)
            fprintf(stderr,"%d 0x%016"PRIx64" (0x%08x)", priv, last_pc, insn_raw);
        else
            fprintf(stderr,"%d 0x%016"PRIx64" (0x%08x)", priv, last_pc, (uint16_t) insn_raw);

        uint64_t dummy1, dummy2;
        int iregno = virt_machine_get_most_recently_written_reg(m, &dummy1);
        int fregno = virt_machine_get_most_recently_written_fp_reg(m, &dummy2);

        if (iregno > 0)
            fprintf(stderr," x%2d 0x%016"PRIx64, iregno, virt_machine_get_reg(m, iregno));
        else if (fregno >= 0)
            fprintf(stderr," f%2d 0x%016"PRIx64, fregno, virt_machine_get_fpreg(m, fregno));

        putc('\n', stderr);
    }

    return keep_going;
}

int main(int argc, char **argv)
{
#ifdef REGRESS_RISCV_COSIM
    riscvemu_cosim_state_t *costate = 0;
    costate = riscvemu_cosim_init(argc, argv);
    do ; while (!riscvemu_cosim_step(costate, 0, 0, 0, 0, false));
#else
    VirtMachine *m = virt_machine_main(argc, argv);
    int keep_going;
    do {
        keep_going = iterate_core(m);
    } while (keep_going);

    if (m->trace > m->maxinsns) {
        // Dump trace after --save for debug. It should match after --load
        for (int i = m->trace - m->maxinsns; i > 0; --i) {
            iterate_core(m);
        }
    }

    fprintf(stderr,"\nPower off.\n");

    virt_machine_end(m);
#endif

    return 0;
}
