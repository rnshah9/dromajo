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
#include "machine.h"
#include "riscv_cpu.h"

int main(int argc, char **argv)
{
    VirtMachine *m = virt_machine_main(argc, argv);
    uint64_t last_pc = 0;
    uint64_t instret = virt_machine_get_instret(m);
    int keep_going;

    do {
        last_pc = virt_machine_get_pc(m);
        int priv = virt_machine_get_priv_level(m);

        uint32_t insn_raw = 0;
        virt_machine_read_insn(m, &insn_raw, last_pc);

        keep_going = virt_machine_run(m);

        uint64_t prev_instret = instret;
        instret = virt_machine_get_instret(m);

        if (prev_instret == instret)
            continue;

        //printf("%6ld ", instret);
        if ((insn_raw & 3) == 3)
            printf("%d 0x%016"PRIx64" (0x%08x)", priv, last_pc, insn_raw);
        else
            printf("%d 0x%016"PRIx64" (0x%04x)", priv, last_pc, (uint16_t) insn_raw);

        uint64_t regno_ts;
        int regno = virt_machine_get_most_recently_written_reg(m, &regno_ts);
        if (regno > 0 && prev_instret == regno_ts)
            printf(" x%2d 0x%016lx", regno,
                   virt_machine_get_reg(m, regno));

        regno = virt_machine_get_most_recently_written_fp_reg(m, &regno_ts);
        if (regno >= 0 && prev_instret == regno_ts)
            printf(" f%2d 0x%016"PRIx64, regno,
                   virt_machine_get_fpreg(m, regno));

        putchar('\n');
    } while (keep_going);

    printf("\nPower off.\n");

    virt_machine_end(m);

    return 0;
}
