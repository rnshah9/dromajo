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
#include <linux/if_tun.h>
#include <sys/stat.h>
#include <signal.h>

#include "cutils.h"
#include "iomem.h"
#include "virtio.h"
#include "machine.h"
#include "riscv_cpu.h"

int main(int argc, char **argv)
{
    VirtMachine *m = virt_machine_main(argc, argv, TRUE);

    uint64_t last_pc = 0;

    while (virt_machine_read_htif_tohost(m) == 0 && virt_machine_get_pc(m) != last_pc) {
        last_pc = virt_machine_get_pc(m);
        int priv = 3; // XXX extract the actual value from RISCVEMU

        uint32_t insn_raw = 0;
        virt_machine_read_insn(m, &insn_raw, last_pc);
        int rd = (insn_raw >> 7) & 0x1F;
        uint64_t old_value = (uint64_t)virt_machine_get_reg(m,rd);
        uint64_t old_fvalue = (uint64_t)virt_machine_get_fpreg(m,rd);
        virt_machine_run(m);
        uint64_t new_value = (uint64_t)virt_machine_get_reg(m,rd);
        uint64_t new_fvalue = (uint64_t)virt_machine_get_fpreg(m,rd);

        if (virt_machine_get_pending_exception(m) >= 0)
            continue;

        printf("%d 0x%016jx (0x%08x)", priv, last_pc, insn_raw);
        if (old_value != new_value) // XXX not ideal
            printf(" x%2d 0x%016jx", rd, new_value);
        if (old_fvalue != new_fvalue) // XXX not ideal
            printf(" f%2d 0x%016jx", rd, new_fvalue);
        putchar('\n');
    }

    virt_machine_end(m);

    return 0;
}
