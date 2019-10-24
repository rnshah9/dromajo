//******************************************************************************
// Copyright (C) 2018,2019, Esperanto Technologies Inc.
// The copyright to the computer program(s) herein is the
// property of Esperanto Technologies, Inc. All Rights Reserved.
// The program(s) may be used and/or copied only with
// the written permission of Esperanto Technologies and
// in accordance with the terms and conditions stipulated in the
// agreement/contract under which the program(s) have been supplied.
//------------------------------------------------------------------------------

#ifndef RISCV_MACHINE_H
#define RISCV_MACHINE_H

#include "virtio.h"
#include "machine.h"
#include "riscv_cpu.h"

#define MAX_CPUS  8

/* Hooks */
typedef struct RISCVMachineHooks {
    /* Returns -1 if invalid CSR, 0 if OK. */
    int (*csr_read) (RISCVCPUState *s, uint32_t csr, uint64_t *pval);
    int (*csr_write)(RISCVCPUState *s, uint32_t csr, uint64_t   val);
} RISCVMachineHooks;

struct RISCVMachine {
    VirtMachine common;
    RISCVMachineHooks hooks;
    PhysMemoryMap *mem_map;
    RISCVCPUState *cpu_state[MAX_CPUS];
    int      ncpus;
    uint64_t ram_size;
    uint64_t ram_base_addr;
    /* PLIC */
    uint32_t plic_pending_irq;
    uint32_t plic_served_irq;
    IRQSignal plic_irq[32]; /* IRQ 0 is not used */

    /* HTIF */
    uint64_t htif_tohost_addr;

    VIRTIODevice *keyboard_dev;
    VIRTIODevice *mouse_dev;

    int virtio_count;

    /* MMIO range (for co-simulation only) */
    uint64_t mmio_start;
    uint64_t mmio_end;
};

#define CLINT_BASE_ADDR 0x02000000
#define CLINT_SIZE      0x000c0000
#define RTC_FREQ_DIV 16 /* arbitrary, relative to CPU freq to have a
                           10 MHz frequency */

#endif
