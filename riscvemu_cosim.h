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
int riscvemu_cosim_step(riscvemu_cosim_state_t *riscvemu_cosim_state,
                        int                     hartid,
                        uint64_t                dut_pc,
                        uint32_t                dut_insn,
                        uint64_t                dut_wdata,
                        int                     dut_ghr_ena,
                        uint64_t                dut_ghr0,  // ghistory[63: 0]
                        uint64_t                dut_ghr1,  // ghistory[89:64]
                        uint64_t                mstatus,
                        bool                    check);

/*
 * riscvemu_cosim_raise_trap --
 *
 * DUT raises a trap (exception or interrupt) and provides the cause.
 * MSB indicates an asynchronous interrupt, synchronous exception
 * otherwise.
 *
 */
void riscvemu_cosim_raise_trap(riscvemu_cosim_state_t *state, int hartid, int64_t cause);
#ifdef __cplusplus
} // extern C
#endif

#endif
