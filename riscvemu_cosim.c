//******************************************************************************
// Copyright (C) 2018,2019, Esperanto Technologies Inc.
// The copyright to the computer program(s) herein is the
// property of Esperanto Technologies, Inc. All Rights Reserved.
// The program(s) may be used and/or copied only with
// the written permission of Esperanto Technologies and
// in accordance with the terms and conditions stipulated in the
// agreement/contract under which the program(s) have been supplied.
//------------------------------------------------------------------------------

/*
 * API for RISCVEMU-based cosimulation
 */

#include "riscvemu.h"
#include "riscvemu_cosim.h"
#include "cutils.h"
#include "iomem.h"
#include "riscv_cpu.h"
#include "riscv_machine.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

/*
 * riscvemu_cosim_init --
 *
 * Creates and initialize the state of the RISC-V ISA golden model
 * Returns NULL upon failure.
 */
riscvemu_cosim_state_t *riscvemu_cosim_init(int argc, char *argv[])
{
    VirtMachine *m = virt_machine_main(argc, argv);

    m->pending_interrupt = -1;
    m->pending_exception = -1;

    return (riscvemu_cosim_state_t *)m;
}

static bool is_store_conditional(uint32_t insn)
{
    int opcode = insn & 0x7f, funct3 = insn >> 12 & 7;
    return opcode == 0x2f && insn >> 27 == 3 && (funct3 == 2 || funct3 == 3);
}

static inline uint32_t get_field1(uint32_t val, int src_pos,
                                  int dst_pos, int dst_pos_max)
{
    int mask;
    assert(dst_pos_max >= dst_pos);
    mask = ((1 << (dst_pos_max - dst_pos + 1)) - 1) << dst_pos;
    if (dst_pos >= src_pos)
        return (val << (dst_pos - src_pos)) & mask;
    else
        return (val >> (src_pos - dst_pos)) & mask;
}

/* detect AMO instruction, including LR, but excluding SC */
static inline bool is_amo(uint32_t insn)
{
    int opcode = insn & 0x7f;
    if (opcode != 0x2f)
        return false;

    switch (insn >> 27) {
    case 1: /* amiswap.w */
    case 2: /* lr.w */
    case 0: /* amoadd.w */
    case 4: /* amoxor.w */
    case 0xc: /* amoand.w */
    case 0x8: /* amoor.w */
    case 0x10: /* amomin.w */
    case 0x14: /* amomax.w */
    case 0x18: /* amominu.w */
    case 0x1c: /* amomaxu.w */
        return true;
    default:
        return false;
    }
}

/*
 * is_mmio_load() --
 * calculated the effective address and check if the physical backing
 * is MMIO space.  NB: get_phys_addr() is the identity if the CPU is
 * running without virtual memory enabled.
 */
static inline bool is_mmio_load(RISCVCPUState *s,
                                int            reg,
                                int            offset,
                                uint64_t       mmio_start,
                                uint64_t       mmio_end)
{
    uint64_t pa;
    uint64_t va = riscv_get_reg_previous(s, reg) + offset;
    return
        !riscv_cpu_get_phys_addr(s, va, ACCESS_READ, &pa) &&
        mmio_start <= pa && pa < mmio_end;
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
static inline void handle_dut_overrides(RISCVCPUState *s,
                                        uint64_t mmio_start,
                                        uint64_t mmio_end,
                                        int priv,
                                        uint64_t pc, uint32_t insn,
                                        uint64_t emu_wdata,
                                        uint64_t dut_wdata)
{
    int opcode = insn & 0x7f;
    int csrno  = insn >> 20;
    int rd     = (insn >> 7) & 0x1f;
    int rdc    = ((insn >> 2) & 7) + 8;
    int reg, offset;

    /* Catch reads from CSR mcycle, ucycle, instret, hpmcounters,
     * hpmoverflows, mip, and sip.
     * If the destination register is x0 then it is actually a csr-write
     */
    if (opcode == 0x73 && rd != 0 &&
        (0xB00 <= csrno && csrno < 0xB20 ||
         0xC00 <= csrno && csrno < 0xC20 ||
         (csrno == CSR_ET_MCIP ||
          csrno == CSR_ET_SCIP ||
          csrno == CSR_ET_UCIP ||
          csrno == 0x344 /* mip */ ||
          csrno == 0x144 /* sip */)))
        riscv_set_reg(s, rd, dut_wdata);

    /* Catch loads and amo from MMIO space */
    if ((opcode == 3 || is_amo(insn)) && rd != 0) {
        reg = (insn >> 15) & 0x1f;
        offset = opcode == 3 ? (int32_t) insn >> 20 : 0;
    } else if ((insn & 0xE003) == 0x6000 && rdc != 0) {
        // c.ld  011  uimm[5:3] rs1'[2:0]       uimm[7:6] rd'[2:0] 00
        reg = ((insn >> 7) & 7) + 8;
        offset = get_field1(insn, 10, 3, 5) | get_field1(insn, 5, 6, 7);
        rd = rdc;
    } else if ((insn & 0xE003) == 0x4000 && rdc != 0) {
        // c.lw  010  uimm[5:3] rs1'[2:0] uimm[2] uimm[6] rd'[2:0] 00
        reg = ((insn >> 7) & 7) + 8;
        offset = (get_field1(insn, 10, 3, 5) |
                  get_field1(insn,  6, 2, 2) |
                  get_field1(insn,  5, 6, 6));
        rd = rdc;
    } else
        return;

    if (is_mmio_load(s, reg, offset, mmio_start, mmio_end)) {
        //fprintf(riscvemu_stderr, "Overriding mmio c.lw (%lx)\n", addr);
        riscv_set_reg(s, rd, dut_wdata);
    }
}

/*
 * riscvemu_cosim_raise_trap --
 *
 * DUT raises a trap (exception or interrupt) and provides the cause.
 * MSB indicates an asynchronous interrupt, synchronous exception
 * otherwise.
 */
void riscvemu_cosim_raise_trap(riscvemu_cosim_state_t *state, int64_t cause)
{
    VirtMachine *m = (VirtMachine  *)state;

    if (cause < 0) {
        assert(m->pending_interrupt == -1); // XXX RTLMAX-434
        m->pending_interrupt = cause & 63;
        fprintf(riscvemu_stderr, "DUT raised interrupt %d\n", m->pending_interrupt);
    } else {
        m->pending_exception = cause;
    }
}

/* cosim_history --
 *
 * Simulate up to 128 bits of global history. Currently requires
 * a manual description of a single hashing function to compare
 * against.
 *
 */

// get a bit from the number: num[idx]
static inline uint64_t get_bit(uint64_t num, int idx)
{
    return (num >> idx) & 1;
}

// generate a bit mask
static inline uint64_t get_mask(int size)
{
    assert(size != 64);
    return (1 << size) - 1;
}

// get a bit-slice from the number: num[hi,lo]
static inline uint64_t get_range(uint64_t num, int hi, int lo)
{
    return (num >> lo) & get_mask(hi - lo + 1);
}

static void cosim_history(RISCVCPUState *s,
                          uint64_t       dut_pc,
                          uint64_t       dut_ghr0,  // ghistory[63: 0]
                          uint64_t       dut_ghr1,  // ghistory[89:64]
                          int           *exit_code)
{
    // keeping simulated state here as statics is pretty disgusting
    static uint64_t emu_ghr0, emu_ghr1;

    /* Step 1: Compare GHR betweeen EMU and DUT. */

    //fprintf(riscvemu_stderr, "   [emu] GHR %016"PRIx64"%016"PRIx64"\n", emu_ghr1, emu_ghr0);
    //fprintf(riscvemu_stderr, "   [dut] GHR %016"PRIx64"%016"PRIx64"\n", dut_ghr1, dut_ghr0);

    if (dut_ghr0 != emu_ghr0 || dut_ghr1 != emu_ghr1) {
        fprintf(riscvemu_stderr, "[error] EMU GHR %016"PRIx64"%016"PRIx64" != DUT GHR %016"PRIx64"%016"PRIx64"\n",
                emu_ghr1, emu_ghr0, dut_ghr1, dut_ghr0);
        *exit_code = 0x1FFF;
    }

    uint64_t emu_ghr0_old = emu_ghr0;

    /* Step 2: Compute GHR for the *next* instruction. */

    RISCVCTFInfo info;
    uint64_t target_pc;

    riscv_get_ctf_info(s, &info);
    riscv_get_ctf_target(s, &target_pc);

    /* Cosimulate the global branch history */
    if (info != ctf_nop) {
        // NB, cft is on the just executed instruction, thus insn_addr is the previous pc
#if 0
        // a very simple hash function; easy for debugging.
        int histlen = 90;
        int shamt = 4;
        emu_ghr1 <<= shamt;
        emu_ghr1 |= emu_ghr0 >> (64-shamt);
        emu_ghr0 = emu_ghr0 << shamt | ((target_pc >> 0) & 0xf);
#endif
#if 1
        // Hard-coded hash function from maxion.
        int histlen = 90; // must be < 128 for cosim reasons.
        int sz0 = 6; // must be even.
        int szh = sz0/2;
        int min = (2*sz0 + szh + 13);

        int pc = target_pc >> 1; // Remove lsb (always zero).
        int foldpc = (pc >> 17) ^ pc;
        int o0 = get_range(emu_ghr0, sz0-1, 0); // old(sz0-1,0)
        int o1 = get_range(emu_ghr0, 2*sz0-1, sz0) ; // old(2*sz0-1,sz0)
        int o2 = get_range(emu_ghr0, 2*sz0+szh, 2*sz0); // old(2*sz0+szh,2*sz0)

        int h0  = foldpc & get_mask(sz0); // foldpc(sz0-1,0)
        int h1  = o0;
        int h2  = (o1 ^ (o1 >> szh)) & get_mask(szh+1); // (o1 ^ (o1 >> (sz0/2).U))(sz0/2-1,0)
        int h3  = (o2 ^ (o2 >> 2)) & get_mask(2); // (o2 ^ (o2 >> 2))(1,0)
        int h10 = get_bit(emu_ghr0, 27) ^ get_bit(emu_ghr0, 26); // fold o9's 2-bits down to 1-bit

        emu_ghr1 <<= 1;
        emu_ghr1 |= get_bit(emu_ghr0, 63);
#endif

        // min = h0.getWidth + h1.getWidth + h2.getWidth + h3.getWidth + 10
        // ret := Cat(old(history_length-1, min), h10, old(25,16), h3, h2, h1, h0)
        emu_ghr0 &= ~((1 << min) - 1);
        emu_ghr0 = \
            (emu_ghr0 << 1) |
            (h10 << (2*sz0 + szh + 13)) |
            (get_range(emu_ghr0_old, 25, 16) << (2*sz0 + szh + 3)) |
            (h3 << (2*sz0 + szh + 1)) |
            (h2 << 2*sz0) |
            (h1 << sz0) |
            (h0);

        // Clear out high-order bits, based on hard-coded history length.
        if (histlen <= 64) {
            emu_ghr1 = 0;
            emu_ghr0 &= (1L << histlen) - 1;
        } else {
            emu_ghr1 &= (1L << (histlen-64)) - 1;
        }
    }
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
                        uint64_t                dut_pc,
                        uint32_t                dut_insn,
                        uint64_t                dut_wdata,
                        int                     dut_ghr_ena,
                        uint64_t                dut_ghr0,  // ghistory[63: 0]
                        uint64_t                dut_ghr1,  // ghistory[89:64]
                        bool                    check)
{
    VirtMachine   *m = (VirtMachine  *)riscvemu_cosim_state;
    RISCVMachine  *r = (RISCVMachine *)m;
    RISCVCPUState *s = r->cpu_state;
    uint64_t emu_pc, emu_wdata = 0;
    int      emu_priv;
    uint32_t emu_insn;
    bool     emu_wrote_data = false;
    int      exit_code = 0;
    bool     verbose = true;
    uint64_t dummy1, dummy2;
    int      iregno, fregno;

    /* Succeed after N instructions without failure. */
    if (m->maxinsns == 0) {
        return 1;
    }

    m->maxinsns--;

    if (riscv_terminated(s)) {
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
        if ((emu_insn & 3) != 3)
            emu_insn &= 0xFFFF;

        if (emu_pc == dut_pc && emu_insn == dut_insn &&
            is_store_conditional(emu_insn) && dut_wdata != 0) {

            /* When DUT fails an SC, we must simulate the same behavior */
            iregno = emu_insn >> 7 & 0x1f;
            if (iregno > 0)
                riscv_set_reg(s, iregno, dut_wdata);
            riscv_set_pc(s, emu_pc + 4);
            break;
        }

        if (m->pending_interrupt != -1 && m->pending_exception != -1) {
            /* ARCHSIM-301: On the DUT, the interrupt can race the exception.
               Let's try to match that behavior */

            fprintf(riscvemu_stderr, "DUT also raised exception %d\n", m->pending_exception);
            riscv_cpu_interp64(s, 1); // Advance into the exception

            int cause = s->priv == PRV_S ? s->scause : s->mcause;

            if (m->pending_exception != cause) {
                char priv = s->priv["US?M"];

                /* Unfortunately, handling the error case is awkward,
                 * so we just exit from here */

                fprintf(riscvemu_stderr, "%d 0x%016"PRIx64" ", emu_priv, emu_pc);
                fprintf(riscvemu_stderr, "(0x%08x) ", emu_insn);
                fprintf(riscvemu_stderr,
                        "[error] EMU %cCAUSE %d != DUT %cCAUSE %d\n",
                        priv, cause, priv, m->pending_exception);

                return 0x1FFF;
            }
        }

        if (m->pending_interrupt != -1)
            riscv_cpu_set_mip(s, riscv_cpu_get_mip(s) | 1 << m->pending_interrupt);

        m->pending_interrupt = -1;
        m->pending_exception = -1;

        if (riscv_cpu_interp64(s, 1) != 0) {
            iregno = riscv_get_most_recently_written_reg(s, &dummy1);
            fregno = riscv_get_most_recently_written_fp_reg(s, &dummy2);
            break;
        }
    }

    if (check)
        handle_dut_overrides(s, r->mmio_start, r->mmio_end,
                             emu_priv, emu_pc, emu_insn, emu_wdata, dut_wdata);

    if (verbose) {
        fprintf(riscvemu_stderr, "%d 0x%016"PRIx64" ", emu_priv, emu_pc);
        fprintf(riscvemu_stderr, "(0x%08x) ", emu_insn);
    }

    if (iregno > 0) {
        emu_wdata = riscv_get_reg(s, iregno);
        emu_wrote_data = 1;
        if (verbose)
            fprintf(riscvemu_stderr, "x%-2d 0x%016"PRIx64, iregno, emu_wdata);
    } else if (fregno >= 0) {
        emu_wdata = riscv_get_fpreg(s, fregno);
        emu_wrote_data = 1;
        if (verbose)
            fprintf(riscvemu_stderr, "f%-2d 0x%016"PRIx64, fregno, emu_wdata);
    } else
        fprintf(riscvemu_stderr, "                      ");

    if (verbose)
        fprintf(riscvemu_stderr, " DASM(0x%08x)\n", emu_insn);

    if (!check)
        return 0;


    if (dut_pc != emu_pc) {
        fprintf(riscvemu_stderr, "[error] EMU PC %016"PRIx64" != DUT PC %016"PRIx64"\n",
                emu_pc, dut_pc);
        exit_code =  0x1FFF;
    }

    if (emu_insn != dut_insn && (emu_insn & 3) == 3) {
        fprintf(riscvemu_stderr, "[error] EMU INSN %08x != DUT INSN %08x\n",
                emu_insn, dut_insn);
        exit_code = 0x1FFF;
    }

    if (dut_wdata != emu_wdata && emu_wrote_data) {
        fprintf(riscvemu_stderr, "[error] EMU WDATA %016"PRIx64" != DUT WDATA %016"PRIx64"\n",
                emu_wdata, dut_wdata);
        exit_code = 0x1FFF;
    }

    if (exit_code == 0)
        riscv_cpu_sync_regs(s);

    if (!dut_ghr_ena)
        return exit_code;


    cosim_history(s, dut_pc, dut_ghr0, dut_ghr1, &exit_code);

    return exit_code;
}
