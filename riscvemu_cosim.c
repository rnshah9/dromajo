/*
 * API for RISCVEMU-based cosimulation
 *
 * Copyright (c) 2018 Esperanto Technologies
 */

#include "riscvemu_cosim.h"
#include "cutils.h"
#include "iomem.h"
#include "riscv_cpu.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

/*
 * riscvemu_cosim_init --
 *
 * creates and initialize the state of the RISC-V ISA golden model
 */
riscvemu_cosim_state_t *riscvemu_cosim_init(int argc, char *argv[])
{
    return (riscvemu_cosim_state_t *)virt_machine_main(argc, argv);
}

/*
 * riscvemu_cosim_step --
 *
 * executes exactly one instruction in the golden model and returns
 * zero if the supplied expected values match.  Caveat: the DUT
 * provides the instructions bit after expansion, so this is only
 * matched on non-compressed instruction.
 *
 * There are a number of situations where the model cannot match the
 * DUT, such as loads from IO devices, interrupts, and CSRs cycle,
 * time, and instret.  For all these cases the model will override
 * with the expected values.
 *
 * The `intr_pending` flag is used to communicate that the DUT will
 * take an interrupt in the next cycle.
 */
int riscvemu_cosim_step(riscvemu_cosim_state_t *riscvemu_cosim_state,
                        uint64_t dut_pc,    uint32_t dut_insn,
                        uint64_t dut_wdata, int dut_intr_pending,
                        bool check)
{
    VirtMachine   *m = (VirtMachine   *)riscvemu_cosim_state;
    RISCVCPUState *s = ((RISCVMachine *)m)->cpu_state;
    uint64_t emu_pc, emu_wdata = 0;
    int      emu_priv;
    uint32_t emu_insn;
    bool     emu_wrote_data = false;
    int      mismatch = 0;
    int      riscv_cpu_interp64(RISCVCPUState *s, int n_cycles);

    /*
     * Execute one instruction in the simulator.  Because exceptions
     * may fire, the current instruction may not be executed, thus we
     * have to iterate until one does.
     */
    for (;;) {
        emu_priv = virt_machine_get_priv_level(m);
        emu_pc   = riscv_get_pc(s);
        virt_machine_read_insn(m, &emu_insn, emu_pc);
        if (riscv_cpu_interp64(s, 1) != 0)
            break;
    };

    fprintf(stderr,"%d %016"PRIx64" ", emu_priv, emu_pc);

    uint64_t dummy1, dummy2;
    int iregno = virt_machine_get_most_recently_written_reg(m, &dummy1);
    int fregno = virt_machine_get_most_recently_written_fp_reg(m, &dummy2);

    if (iregno > 0) {
        emu_wdata = virt_machine_get_reg(m, iregno);
        emu_wrote_data = 1;
        fprintf(stderr, "x%-2d %016"PRIx64, iregno, emu_wdata);
    } else if (fregno >= 0) {
        emu_wdata = virt_machine_get_fpreg(m, fregno);
        emu_wrote_data = 1;
        fprintf(stderr, "f%-2d %016"PRIx64, fregno, emu_wdata);
    } else
        fprintf(stderr, "                    ");

    if ((emu_insn & 3) == 3)
        fprintf(stderr, " DASM(0x%08x)\n", emu_insn);
    else
        fprintf(stderr, " DASM(0x%04x)\n", (uint16_t)emu_insn);

    if (check) {
        if (dut_pc != emu_pc) {
            fprintf(stderr, "[error] EMU PC %016"PRIx64" != DUT PC %016"PRIx64"\n",
                    emu_pc, dut_pc);
            mismatch = 1;
        }

        if (emu_insn != dut_insn && (emu_insn & 3) == 3) {
            fprintf(stderr, "[error] EMU INSN %08x != DUT INSN %08x\n",
                    emu_insn, dut_insn);
            mismatch = 1;
        }

        if (dut_wdata != emu_wdata && emu_wrote_data) {
            fprintf(stderr, "[error] EMU WDATA %016"PRIx64" != DUT WDATA %016"PRIx64"\n",
                    emu_wdata, dut_wdata);
            mismatch = 1;
        }
    }

    return mismatch;
}
