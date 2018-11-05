/*
 * API for RISCVEMU-based cosimulation
 *
 * Copyright (c) 2018 Esperanto Technologies
 */

#ifndef _RISCVEMU_COSIM_H
#define _RISCVEMU_COSIM_H 1

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct riscvemu_cosim_state_st riscvemu_cosim_state_t;

/*
 * riscvemu_cosim_init --
 *
 * creates and initialize the state of the RISC-V ISA golden model
 */
riscvemu_cosim_state_t *riscvemu_cosim_init(int argc, char *argv[]);

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
int riscvemu_cosim_step(riscvemu_cosim_state_t *state,
                        uint64_t pc, uint32_t insn, uint64_t wdata,
                        int intr_pending, bool check);
#ifdef __cplusplus
} // extern C
#endif

#endif
