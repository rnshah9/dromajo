/*
 * API for RISCVEMU-based cosimulation
 *
 * Copyright (c) 2018 Esperanto Technologies
 */

#include "riscvemu_cosim.h"
#include "cutils.h"
#include "iomem.h"
#include "riscv_cpu.h"
#include "riscv_machine.h"
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

static bool is_store_conditional(uint32_t insn)
{
    int opcode = insn & 0x7f, funct3 = (insn >> 12) & 7;
    return opcode == 0x2f && insn >> 27 == 3 && (funct3 == 2 || funct3 == 3);
}

/*
 * handle_dut_overrides --
 *
 * Certain sequences cannot be simulated faithfully so this function
 * is responsible for detecting them and overriding the simulation
 * with the DUT result.  Cases include interrupts, loads from MMIO
 * space, and certain CRSs like cycle and time.
 *
 * Right now we handle just mcycle.
 */
static void handle_dut_overrides(RISCVCPUState *s,
                                 uint64_t pc, uint32_t insn,
                                 uint64_t emu_wdata,
                                 uint64_t dut_wdata,
                                 int dut_intr_pending)
{
    int opcode = insn & 0x7f;
    int csrno  = insn >> 20;
    int rd     = (insn >> 7) & 0x1f;

    /* Catch reads from CSR mcycle, ucycle, instret, hpmcounters,
     * If the destination register is x0 then it is actually a csr-write
     */
    if (opcode == 0x73 && rd != 0 &&
        (0xB00 <= csrno && csrno < 0xB20 ||
         0xC00 <= csrno && csrno < 0xC20))
        riscv_set_reg(s, rd, dut_wdata);
}

/*
 * riscvemu_cosim_step --
 *
 * executes exactly one instruction in the golden model and returns
 * zero if the supplied expected values match and execution should
 * continue.  A non-zero value signals termination with the exit code
 * being the upper bits (ie., all but LSB).  Caveat: the DUT provides
 * the instructions bit after expansion, so this is only matched on
 * non-compressed instruction.
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
    RISCVMachine  *r = (RISCVMachine *)m;
    RISCVCPUState *s = r->cpu_state;
    uint64_t emu_pc, emu_wdata = 0;
    int      emu_priv;
    uint32_t emu_insn;
    bool     emu_wrote_data = false;
    int      exit_code = 0;
    int      riscv_cpu_interp64(RISCVCPUState *s, int n_cycles);
    bool     verbose = true;
    uint64_t dummy1, dummy2;
    int      iregno, fregno;

    if (m->maxinsns-- == 0) {
        /* Succeed after N instructions without failure. */
        return 1;
    }

    /*
     * Execute one instruction in the simulator.  Because exceptions
     * may fire, the current instruction may not be executed, thus we
     * have to iterate until one does.
     */
    iregno = -1;
    fregno = -1;

    for (;;) {
        emu_priv = riscv_get_priv_level(s);
        emu_pc   = riscv_get_pc(s);
        riscv_read_insn(s, &emu_insn, emu_pc);

        if (emu_pc == dut_pc && emu_insn == dut_insn &&
            is_store_conditional(emu_insn) && dut_wdata != 0) {

            /* When DUT fails an SC, we must simulate the same behavior */
            iregno = (emu_insn >> 7) & 0x1f;
            if (iregno > 0)
                riscv_set_reg(s, iregno, dut_wdata);
            riscv_set_pc(s, emu_pc + 4);
            break;
        }

        if (riscv_cpu_interp64(s, 1) != 0) {
            iregno = riscv_get_most_recently_written_reg(s, &dummy1);
            fregno = riscv_get_most_recently_written_fp_reg(s, &dummy2);
            break;
        }
    };

    if (check)
        handle_dut_overrides(s, emu_pc, emu_insn, emu_wdata,
                             dut_wdata, dut_intr_pending);

    if (verbose)
        fprintf(stderr,"%d 0x%016"PRIx64" ", emu_priv, emu_pc);

    if ((emu_insn & 3) == 3)
        fprintf(stderr, "(0x%08x) ", emu_insn);
    else
        fprintf(stderr, "(0x%08x) ", (uint16_t)emu_insn);

    if (iregno > 0) {
        emu_wdata = riscv_get_reg(s, iregno);
        emu_wrote_data = 1;
        if (verbose)
            fprintf(stderr, "x%-2d 0x%016"PRIx64, iregno, emu_wdata);
    } else if (fregno >= 0) {
        emu_wdata = riscv_get_fpreg(s, fregno);
        emu_wrote_data = 1;
        if (verbose)
            fprintf(stderr, "f%-2d 0x%016"PRIx64, fregno, emu_wdata);
    } else
        fprintf(stderr, "                      ");

    if (verbose)
        if ((emu_insn & 3) == 3)
            fprintf(stderr, " DASM(0x%08x)\n", emu_insn);
        else
            fprintf(stderr, " DASM(0x%04x)\n", (uint16_t)emu_insn);

    if (check) {
        if (dut_pc != emu_pc) {
            fprintf(stderr, "[error] EMU PC %016"PRIx64" != DUT PC %016"PRIx64"\n",
                    emu_pc, dut_pc);
            exit_code =  0x1FFF;
        }

        if (emu_insn != dut_insn && (emu_insn & 3) == 3) {
            fprintf(stderr, "[error] EMU INSN %08x != DUT INSN %08x\n",
                    emu_insn, dut_insn);
            exit_code = 0x1FFF;
        }

        if (dut_wdata != emu_wdata && emu_wrote_data) {
            fprintf(stderr, "[error] EMU WDATA %016"PRIx64" != DUT WDATA %016"PRIx64"\n",
                    emu_wdata, dut_wdata);
            exit_code = 0x1FFF;
        }
    }

    return exit_code;
}
