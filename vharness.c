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
//#include "machine.h"
#include "riscv_cpu.h"

int iterate_core(VirtMachine *m)
{
    uint64_t last_pc = virt_machine_get_pc(m);
    int priv = virt_machine_get_priv_level(m);

    uint32_t insn_raw = 0;
    virt_machine_read_insn(m, &insn_raw, last_pc);

    uint64_t prev_instret = virt_machine_get_instret(m);

    int keep_going = virt_machine_run(m);

    uint64_t instret = virt_machine_get_instret(m);

    if (prev_instret == instret && last_pc == virt_machine_get_pc(m))
        return 1;

    if (instret < m->trace && instret < m->maxinsns)
        return keep_going;

    if ((insn_raw & 3) == 3)
        fprintf(stderr,"%d 0x%016"PRIx64" (0x%08x)", priv, last_pc, insn_raw);
    else
        fprintf(stderr,"%d 0x%016"PRIx64" (0x%04x)", priv, last_pc, (uint16_t) insn_raw);

    uint64_t regno_ts;
    int regno = virt_machine_get_most_recently_written_reg(m, &regno_ts);
    if (regno > 0 && prev_instret == regno_ts)
        fprintf(stderr," x%2d 0x%016lx", regno, (unsigned long)virt_machine_get_reg(m, regno));

    regno = virt_machine_get_most_recently_written_fp_reg(m, &regno_ts);
    if (regno >= 0 && prev_instret == regno_ts)
        fprintf(stderr," f%2d 0x%016"PRIx64, regno, virt_machine_get_fpreg(m, regno));

    putc('\n',stderr);

    return keep_going;
}

int main(int argc, char **argv)
{
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

    return 0;
}
