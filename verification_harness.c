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
#ifdef CONFIG_CPU_RISCV
#include "riscv_cpu.h"
#endif

VirtMachine *virt_machine_main(int argc, char **argv);
void virt_machine_run(VirtMachine *m);
void virt_machine_end(VirtMachine *m);
void virt_machine_dump_regs(VirtMachine *m);
int  virt_machine_read_insn(VirtMachine *m, uint32_t *insn, uint64_t addr);

void      virt_machine_set_pc(VirtMachine *m, uint64_t pc);
uint64_t  virt_machine_get_pc(VirtMachine *m);
uint64_t  virt_machine_get_reg(VirtMachine *m, int rn);
uint64_t  virt_machine_read_htif_tohost(VirtMachine *m);

int main(int argc, char **argv)
{
  VirtMachine *m = virt_machine_main(argc, argv);

  uint64_t last_pc = 0;

  while (virt_machine_read_htif_tohost(m) == 0 && virt_machine_get_pc(m) != last_pc) {
    last_pc = virt_machine_get_pc(m);

    uint32_t insn_raw = 0;
    virt_machine_read_insn(m, &insn_raw, last_pc);
    int rd = (insn_raw >> 7) & 0x1F;
    long long old_value = (long long)virt_machine_get_reg(m,rd);
    virt_machine_run(m);
    long long new_value = (long long)virt_machine_get_reg(m,rd);

    /* Slightly hackish as I should really find out what the first "3"
     * is for */
    printf("3 0x%016jx (0x%08x)", last_pc, insn_raw);
    if (old_value != new_value) // XXX not ideal
        printf(" x%2d %016llx", rd, new_value);
    putchar('\n');
}

  virt_machine_end(m);

  return 0;
}
