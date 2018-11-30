//******************************************************************************
// Copyright (C) 2018, Esperanto Technologies Inc.
// The copyright to the computer program(s) herein is the
// property of Esperanto Technologies, Inc. All Rights Reserved.
// The program(s) may be used and/or copied only with
// the written permission of Esperanto Technologies and
// in accordance with the terms and conditions stipulated in the
// agreement/contract under which the program(s) have been supplied.
//------------------------------------------------------------------------------

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
 * Creates and initialize the state of the RISC-V ISA golden model
 * Returns NULL upon failure.
 */
riscvemu_cosim_state_t *riscvemu_cosim_init(int argc, char *argv[]);

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
 */
int riscvemu_cosim_step(riscvemu_cosim_state_t *state,
                        uint64_t pc, uint32_t insn, uint64_t wdata,
                        bool check);

/*
 * riscvemu_cosim_raise_interrupt --
 *
 * DUT asynchronously raises interrupt and provides the cause
 *
 */
void riscvemu_cosim_raise_interrupt(riscvemu_cosim_state_t *state,
                                    int cause);

#ifdef __cplusplus
} // extern C
#endif

#endif
