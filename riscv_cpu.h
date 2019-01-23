//******************************************************************************
// Copyright (C) 2018,2019, Esperanto Technologies Inc.
// The copyright to the computer program(s) herein is the
// property of Esperanto Technologies, Inc. All Rights Reserved.
// The program(s) may be used and/or copied only with
// the written permission of Esperanto Technologies and
// in accordance with the terms and conditions stipulated in the
// agreement/contract under which the program(s) have been supplied.
//------------------------------------------------------------------------------

#ifndef RISCV_CPU_H
#define RISCV_CPU_H

/*
 * RISCV CPU emulator
 *
 * Copyright (c) 2016-2017 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "validation_events.h"

#define MIP_USIP (1 << 0)
#define MIP_SSIP (1 << 1)
//#define MIP_HSIP (1 << 2)  Removed in Priv 1.11 (draft)
#define MIP_MSIP (1 << 3)
#define MIP_UTIP (1 << 4)
#define MIP_STIP (1 << 5)
//#define MIP_HTIP (1 << 6)  Removed in Priv 1.11 (draft)
#define MIP_MTIP (1 << 7)
#define MIP_UEIP (1 << 8)
#define MIP_SEIP (1 << 9)
//#define MIP_HEIP (1 << 10)  Removed in Priv 1.11 (draft)
#define MIP_MEIP (1 << 11)

#define MIE_USIE MIP_USIP
#define MIE_SSIE MIP_SSIP
#define MIE_MSIE MIP_MSIP
#define MIE_UTIE MIP_UTIP
#define MIE_STIE MIP_STIP
#define MIE_MTIE MIP_MTIP
#define MIE_UEIE MIP_UEIP
#define MIE_SEIE MIP_SEIP
#define MIE_MEIE MIP_MEIP

#define ROM_SIZE       0x00001000
#define ROM_BASE_ADDR  0x00010000
#define BOOT_BASE_ADDR 0x00010000

// The default RAM base, can be relocated with config "memory_base_addr"
#define RAM_BASE_ADDR  0x80000000

#ifndef FLEN
#define FLEN 64
#endif /* !FLEN */

#define DUMP_INVALID_MEM_ACCESS
#define DUMP_MMU_EXCEPTIONS
//#define DUMP_INTERRUPTS
//#define DUMP_INVALID_CSR
#define DUMP_ILLEGAL_INSTRUCTION
//#define DUMP_EXCEPTIONS
//#define DUMP_CSR
#define CONFIG_LOGFILE
#define CONFIG_SW_MANAGED_A_AND_D 1
#define CONFIG_ALLOW_MISALIGNED_ACCESS 0

#if FLEN > 0
#include "softfp.h"
#endif

#define __exception __attribute__((warn_unused_result))

typedef uint64_t target_ulong;
typedef int64_t target_long;
#define PR_target_ulong "016" PRIx64

/* FLEN is the floating point register width */
#if FLEN > 0
#if FLEN == 32
typedef uint32_t fp_uint;
#define F32_HIGH 0
#elif FLEN == 64
typedef uint64_t fp_uint;
#define F32_HIGH ((fp_uint)-1 << 32)
#define F64_HIGH 0
#elif FLEN == 128
typedef uint128_t fp_uint;
#define F32_HIGH ((fp_uint)-1 << 32)
#define F64_HIGH ((fp_uint)-1 << 64)
#else
#error unsupported FLEN
#endif
#endif

/* MLEN is the maximum memory access width */
#if 64 <= 32 && FLEN <= 32
#define MLEN 32
#elif 64 <= 64 && FLEN <= 64
#define MLEN 64
#else
#define MLEN 128
#endif

#if MLEN == 32
typedef uint32_t mem_uint_t;
#elif MLEN == 64
typedef uint64_t mem_uint_t;
#elif MLEN == 128
typedef uint128_t mem_uint_t;
#else
#unsupported MLEN
#endif

#define TLB_SIZE 256

#define CAUSE_MISALIGNED_FETCH    0x0
#define CAUSE_FAULT_FETCH         0x1
#define CAUSE_ILLEGAL_INSTRUCTION 0x2
#define CAUSE_BREAKPOINT          0x3
#define CAUSE_MISALIGNED_LOAD     0x4
#define CAUSE_FAULT_LOAD          0x5
#define CAUSE_MISALIGNED_STORE    0x6
#define CAUSE_FAULT_STORE         0x7
#define CAUSE_USER_ECALL          0x8
#define CAUSE_SUPERVISOR_ECALL    0x9
#define CAUSE_HYPERVISOR_ECALL    0xa
#define CAUSE_MACHINE_ECALL       0xb
#define CAUSE_FETCH_PAGE_FAULT    0xc
#define CAUSE_LOAD_PAGE_FAULT     0xd
#define CAUSE_STORE_PAGE_FAULT    0xf

#define CAUSE_MASK 0x1f // not including the MSB for interrupt

/* Note: converted to correct bit position at runtime */
#define CAUSE_INTERRUPT  ((uint32_t)1 << 31)

#define PRV_U 0
#define PRV_S 1
#define PRV_H 2
#define PRV_M 3

/* misa CSR */
#define MCPUID_SUPER   (1 << ('S' - 'A'))
#define MCPUID_USER    (1 << ('U' - 'A'))
#define MCPUID_I       (1 << ('I' - 'A'))
#define MCPUID_M       (1 << ('M' - 'A'))
#define MCPUID_A       (1 << ('A' - 'A'))
#define MCPUID_F       (1 << ('F' - 'A'))
#define MCPUID_D       (1 << ('D' - 'A'))
#define MCPUID_Q       (1 << ('Q' - 'A'))
#define MCPUID_C       (1 << ('C' - 'A'))

/* mstatus CSR */

#define MSTATUS_SPIE_SHIFT 5
#define MSTATUS_MPIE_SHIFT 7
#define MSTATUS_SPP_SHIFT 8
#define MSTATUS_MPP_SHIFT 11
#define MSTATUS_FS_SHIFT 13
#define MSTATUS_UXL_SHIFT 32
#define MSTATUS_SXL_SHIFT 34

#define MSTATUS_UIE (1 << 0)
#define MSTATUS_SIE (1 << 1)
#define MSTATUS_HIE (1 << 2)
#define MSTATUS_MIE (1 << 3)
#define MSTATUS_UPIE (1 << 4)
#define MSTATUS_SPIE (1 << MSTATUS_SPIE_SHIFT)
#define MSTATUS_HPIE (1 << 6)
#define MSTATUS_MPIE (1 << MSTATUS_MPIE_SHIFT)
#define MSTATUS_SPP (1 << MSTATUS_SPP_SHIFT)
#define MSTATUS_HPP (3 << 9)
#define MSTATUS_MPP (3 << MSTATUS_MPP_SHIFT)
#define MSTATUS_FS (3 << MSTATUS_FS_SHIFT)
#define MSTATUS_XS (3 << 15)
#define MSTATUS_MPRV (1 << 17)
#define MSTATUS_SUM (1 << 18)
#define MSTATUS_MXR (1 << 19)
#define MSTATUS_TVM (1 << 20)
#define MSTATUS_TW (1 << 21)
#define MSTATUS_TSR (1 << 22)
#define MSTATUS_UXL_MASK ((uint64_t)3 << MSTATUS_UXL_SHIFT)
#define MSTATUS_SXL_MASK ((uint64_t)3 << MSTATUS_SXL_SHIFT)

#define PG_SHIFT 12
#define PG_MASK ((1 << PG_SHIFT) - 1)

#define ASID_BITS 0

#define SATP_MASK ((15ULL << 60) | (((1ULL << ASID_BITS) - 1) << 44) | ((1ULL << 44) - 1))

#ifndef MAX_TRIGGERS
#define MAX_TRIGGERS 1 // As of right now, Maxion implements one trigger register
#endif

// A few of Debug Trigger Match Control bits (there are many more)
#define MCONTROL_M         (1 << 6)
#define MCONTROL_S         (1 << 4)
#define MCONTROL_U         (1 << 3)
#define MCONTROL_EXECUTE   (1 << 2)
#define MCONTROL_STORE     (1 << 1)
#define MCONTROL_LOAD      (1 << 0)

typedef struct {
    target_ulong vaddr;
    uintptr_t mem_addend;
} TLBEntry;

/* Control-flow summary information */
typedef enum {
    ctf_nop = 1,
    ctf_taken_jump,
    ctf_taken_branch,
    // Indirect jumps come in four variants depending on hits
    // NB: the order is important
    ctf_taken_jalr,
    ctf_taken_jalr_pop,
    ctf_taken_jalr_push,
    ctf_taken_jalr_pop_push,
} RISCVCTFInfo;

struct RISCVCPUState {
    target_ulong pc;
    target_ulong reg[32];
    /* Co-simulation sometimes need to see the value of a register
     * prior to the just excuted instruction. */
    target_ulong reg_prior[32];
    /* reg_ts[x] is the timestamp (in executed instructions) of the most
     * recent definition of the register. */
    uint64_t reg_ts[32];
    int most_recently_written_reg;

#if FLEN > 0
    fp_uint fp_reg[32];
    uint64_t fp_reg_ts[32];
    int most_recently_written_fp_reg;
    uint32_t fflags;
    uint8_t frm;
#endif

    uint8_t priv; /* see PRV_x */
    uint8_t fs; /* MSTATUS_FS value */

    uint64_t insn_counter; // Simulator internal
    uint64_t minstret; // RISCV CSR (updated when insn_counter increases)
    uint64_t mcycle;   // RISCV CSR (updated when insn_counter increases)
    BOOL     stop_the_counter; // Set in debug mode only (cleared after ending Debug)

    BOOL power_down_flag; /* True when the core is idle awaiting
                           * interrupts, does NOT mean terminate
                           * simulation */
    BOOL terminate_simulation;
    int pending_exception; /* used during MMU exception handling */
    target_ulong pending_tval;

    /* CSRs */
    target_ulong mstatus;
    target_ulong mtvec;
    target_ulong mscratch;
    target_ulong mepc;
    target_ulong mcause;
    target_ulong mtval;
    target_ulong mvendorid; /* ro */
    target_ulong marchid; /* ro */
    target_ulong mimpid; /* ro */
    target_ulong mhartid; /* ro */
    uint32_t misa;
    uint32_t mie;
    uint32_t mip;
    uint32_t medeleg;
    uint32_t mideleg;
    uint32_t mcounteren;
    uint32_t tselect;
    target_ulong tdata1[MAX_TRIGGERS];
    target_ulong tdata2[MAX_TRIGGERS];
    target_ulong tdata3[MAX_TRIGGERS];

    target_ulong mhpmevent[32];

    target_ulong stvec;
    target_ulong sscratch;
    target_ulong sepc;
    target_ulong scause;
    target_ulong stval;
    uint64_t satp; /* currently 64 bit physical addresses max */
    uint32_t scounteren;

    target_ulong dcsr; // Debug CSR 0x7b0 (debug spec only)
    target_ulong dpc;  // Debug DPC 0x7b1 (debug spec only)
    target_ulong dscratch;  // Debug dscratch 0x7b2 (debug spec only)

    target_ulong load_res; /* for atomic LR/SC */
    uint32_t  store_repair_val32; /* saving previous value of memory so it can be repaired */
    uint64_t  store_repair_val64;
    uint128_t store_repair_val128;
    target_ulong store_repair_addr; /* saving which address to repair */
    uint64_t last_addr; /* saving previous value of address so it can be repaired */

    PhysMemoryMap *mem_map;

    TLBEntry tlb_read[TLB_SIZE];
    TLBEntry tlb_write[TLB_SIZE];
    TLBEntry tlb_code[TLB_SIZE];

    // User specified, command line argument terminating event
    const char *terminating_event;
    // Benchmark return value
    uint64_t benchmark_exit_code;

    /* Control Flow Info */
    RISCVCTFInfo info;
    target_ulong next_addr; /* the CFI target address-- only valid for CFIs. */
};

typedef struct RISCVCPUState RISCVCPUState;

RISCVCPUState *riscv_cpu_init(PhysMemoryMap *mem_map, const char *term_event);
void riscv_cpu_end(RISCVCPUState *s);
int riscv_cpu_interp(RISCVCPUState *s, int n_cycles);
uint64_t riscv_cpu_get_cycles(RISCVCPUState *s);
void riscv_cpu_set_mip(RISCVCPUState *s, uint32_t mask);
void riscv_cpu_reset_mip(RISCVCPUState *s, uint32_t mask);
uint32_t riscv_cpu_get_mip(RISCVCPUState *s);
BOOL riscv_cpu_get_power_down(RISCVCPUState *s);
uint32_t riscv_cpu_get_misa(RISCVCPUState *s);
void riscv_cpu_flush_tlb_write_range_ram(RISCVCPUState *s,
                                         uint8_t *ram_ptr, size_t ram_size);
void riscv_set_pc(RISCVCPUState *s, uint64_t pc);
uint64_t riscv_get_pc(RISCVCPUState *s);
uint64_t riscv_get_reg(RISCVCPUState *s, int rn);
uint64_t riscv_get_reg_previous(RISCVCPUState *s, int rn);
uint64_t riscv_get_fpreg(RISCVCPUState *s, int rn);
void riscv_set_reg(RISCVCPUState *s, int rn, uint64_t val);
void riscv_dump_regs(RISCVCPUState *s);
int riscv_read_insn(RISCVCPUState *s, uint32_t *insn, uint64_t addr);
int riscv_read_u64(RISCVCPUState *s, uint64_t *data, uint64_t addr);
void riscv_repair_csr(RISCVCPUState *s, uint32_t reg_num, uint64_t csr_num,
                      uint64_t csr_val);
int riscv_repair_store(RISCVCPUState *s, uint32_t reg_num, uint32_t funct3);
int riscv_repair_load(RISCVCPUState *s, uint32_t reg_num, uint64_t reg_val,
                      uint64_t htif_tohost_addr, uint64_t *htif_tohost,
                      uint64_t *htif_fromhost);
void riscv_cpu_sync_regs(RISCVCPUState *s);
int riscv_get_priv_level(RISCVCPUState *s);
int riscv_get_most_recently_written_reg(RISCVCPUState *s,
                                        uint64_t *instret_ts);
int riscv_get_most_recently_written_fp_reg(RISCVCPUState *s,
                                           uint64_t *instret_ts);
void riscv_get_ctf_info(RISCVCPUState *s, RISCVCTFInfo *info);
void riscv_get_ctf_target(RISCVCPUState *s, uint64_t *target);

int riscv_cpu_interp64(RISCVCPUState *s, int n_cycles);
BOOL riscv_terminated(RISCVCPUState *s);

int riscv_benchmark_exit_code(RISCVCPUState *s);

#include "riscv_machine.h"
void riscv_cpu_serialize(RISCVCPUState *s, RISCVMachine *m, const char *dump_name);
void riscv_cpu_deserialize(RISCVCPUState *s, RISCVMachine *m, const char *dump_name);

#endif
