//******************************************************************************
// Copyright (C) 2019, Esperanto Technologies Inc.
// The copyright to the computer program(s) herein is the
// property of Esperanto Technologies, Inc. All Rights Reserved.
// The program(s) may be used and/or copied only with
// the written permission of Esperanto Technologies and
// in accordance with the terms and conditions stipulated in the
// agreement/contract under which the program(s) have been supplied.
//------------------------------------------------------------------------------

#include "dw_apb_uart.h"
#include <stdio.h>
#include <assert.h>

static const char *reg_names[256/4] = {
    "rx buf / div latch lo",
    "intr en / div latch hi",
    "intr id / FIFO ctrl",
    "line control",
    "modem control",
    "line status",
    "modem status",
    "scratch",
    "reserved0",
    "reserved1",
    "reserved2",
    "reserved3",
    "shadow rx/tx buf0",
    "shadow rx/tx buf1",
    "shadow rx/tx buf2",
    "shadow rx/tx buf3",
    "shadow rx/tx buf4",
    "shadow rx/tx buf5",
    "shadow rx/tx buf6",
    "shadow rx/tx buf7",
    "shadow rx/tx buf8",
    "shadow rx/tx buf9",
    "shadow rx/tx buf10",
    "shadow rx/tx buf11",
    "shadow rx/tx buf12",
    "shadow rx/tx buf13",
    "shadow rx/tx buf14",
    "shadow rx/tx buf15",
    "FIFO access",
    "tx FIFO read",
    "rxr FIFO write",
    "uart status",
    "tx FIFO level",
    "rx FIFO level",
    "software reset",
    "shadow RTS",
    "shadow break control",
    "shadow dma mode",
    "shadow FIFO enable",
    "shadow rx trigger",
    "shadow tx trigger",
    "halt Tx",
    "dma software ack",

    [0xf4/4] = "component parameters",
    [0xf8/4] = "component version",
    [0xfc/4] = "component type",
};

/* The most important registers (only ones implemented) */

enum {
    uart_reg_rx_buf     = 0x00,
    uart_reg_intren     = 0x04,
    uart_reg_intrid     = 0x08,
    uart_reg_linecontrol= 0x0c,

    uart_reg_linestatus = 0x14,
    uart_reg_comptype   = 0xfc,
};

/* Configuration parameters at hardware instantiation time (only includes features relevant to sim) */
#define FEATURE_FIFO_MODE              64
#define FEATURE_REG_TIMEOUT_WIDTH       4
#define FEATURE_HC_REG_TIMEOUT_VALUE    0
#define FEATURE_REG_TIMEOUT_VALUE       8
#define FEATURE_UART_RS485_INTERFACE_EN 0
#define FEATURE_UART_9BIT_DATA_EN       0
#define FEATURE_APB_DATA_WIDTH         32
#define FEATURE_MEM_SELECT_USER         1  // == internal
#define FEATURE_SIR_MODE                0 // disabled
#define FEATURE_AFCE_MODE               0
#define FEATURE_THRE_MODE_USER          1 // enabled
#define FEATURE_FIFO_ACCESS             1 // programmable FIFOQ access mode enabled
#define FEATURE_ADDITIONAL_FEATURES     1
#define FEATURE_FIFO_STAT               1
#define FEATURE_SHADOW                  1
#define FEATURE_UART_ADD_ENCODED_PARAMS 1
#define FEATURE_UART_16550_COMPATIBLE   0
#define FEATURE_FRACTIONAL_BAUD_DIVISOR_EN 1
#define FEATURE_DLF_SIZE                4
#define FEATURE_LSR_STATUS_CLEAR        0 // Both RBR Read and LSR Read clears OE, PE, FE, and BI

//#define DEBUG(fmt ...) fprintf(riscvemu_stderr, fmt)
#define DEBUG(fmt ...) (void) 0

uint32_t dw_apb_uart_read(void *opaque, uint32_t offset, int size_log2)
{
    DW_apb_uart_state *s = opaque;
    int res = 0;

    assert(offset % 4 == 0 && offset/4 < 64);

    switch (offset) {
    case uart_reg_rx_buf: // 0x00
        if (s->lcr & (1 << bfoUART_LCR_DLAB)) {
            res = s->div_latch & 255;
        } else {
            res = s->rbr;
            s->lsr &= ~(1 << bfoUART_LSR_DR);

            // XXX more side effects here, opt. drain FIFO when in FIFO mode
            // Read from device maybe

            if (FEATURE_LSR_STATUS_CLEAR == 0)
                s->lsr &= ~30; // Reading clears BI, FE, PE, OE
        }
        break;

    case uart_reg_intren: // 0x04
        if (s->lcr & (1 << bfoUART_LCR_DLAB)) {
            res = s->div_latch >> 8;
        } else {
            res = s->ier;
        }
        break;


    case uart_reg_intrid: // 0x08
        s->iid = 1; // XXX Value After Reset
        res = (s->fcr & (1 << bfoUART_FCR_FIFO_ENABLE) ? 0xc0 : 0) + s->iid;
        break;

    case uart_reg_linecontrol: // 0x0c
        res = s->lcr;
        break;

    case uart_reg_linestatus: // 0x14
        res = s->lsr;
        s->lsr |= (1 << bfoUART_LSR_TEMT) | (1 << bfoUART_LSR_THRE); // TX empty, Holding Empty
        s->lsr &= ~30; // Reading clears BI, FE, PE, OE
        break;

    case uart_reg_comptype: // 0xfc
    default:;
    }

    if (reg_names[offset/4])
        DEBUG("dw_apb_uart_read(0x%02x \"%s\") -> %d\n", offset, reg_names[offset/4], res);
    else
        DEBUG("dw_apb_uart_read(0x%02x, %d) -> %d\n", offset, size_log2, res);

    return res;
}

void dw_apb_uart_write(void *opaque, uint32_t offset, uint32_t val, int size_log2)
{
    DW_apb_uart_state *s = opaque;

    val &= 255;

    assert(offset % 4 == 0 && offset/4 < 64);

    if (reg_names[offset/4] && size_log2 == 2)
        DEBUG("dw_apb_uart_write(0x%02x \"%s\", %d)\n", offset, reg_names[offset/4], val);
    else
        DEBUG("dw_apb_uart_write(0x%02x, %d, %d)\n", offset, val, size_log2);

    switch (offset) {
    case uart_reg_rx_buf: // 0x00
        if (s->lcr & (1 << bfoUART_LCR_DLAB)) {
            s->div_latch = (s->div_latch & ~255) + val;
            DEBUG("     div latch is now %d\n", s->div_latch);
        } else {
            if (s->lsr & (1 << bfoUART_LSR_THRE)) {
                CharacterDevice *cs = s->cs;
                unsigned char ch = val;
                DEBUG("   TRANSMIT '%c' (0x%02x)\n", val, val);
                cs->write_data(cs->opaque, &ch, 1);
                s->lsr &= ~(1 << bfoUART_LSR_THRE); // XXX Assumes non-fifo mode
            } else {
                DEBUG("   OVERFLOW ('%c' (0x%02x))\n", val, val);
                // XXX Oddly it doesn't appear that overflow is registered?
            }
        }
        break;

    case uart_reg_intren: // 0x04
        if (s->lcr & (1 << bfoUART_LCR_DLAB)) {
            s->div_latch = (s->div_latch & 255) + val * 256;
            DEBUG("     div latch is now %d\n", s->div_latch);
        } else {
            s->ier = val & (FEATURE_THRE_MODE_USER ? 0xFF : 0x7F);
        }
        break;

    case uart_reg_intrid: // 0x08
        s->fcr = val;
        for (int i = 0; i < 8; ++i)
            if (s->fcr & (1 << i))
                switch (i) {
                case bfoUART_FCR_FIFO_ENABLE:
                    DEBUG("    FIFO enable\n");
                    break;
                case bfoUART_FCR_RCVR_FIFO_RESET:
                    DEBUG("    receiver FIFO reset\n");
                    break;
                case bfoUART_FCR_XMIT_FIFO_RESET:
                    DEBUG("    transmitter FIFO reset\n");
                    break;
                case bfoUART_FCR_DMA_MODE:
                    DEBUG("    dma mode\n");
                    break;
                case bfoUART_FCR_TX_EMPTY_TRIGGER:
                    DEBUG("    transmitter empty trigger\n");
                    break;
                case bfoUART_FCR_RCVR_TRIGGER:
                    DEBUG("    receiver trigger\n");
                    break;
                default:
                    DEBUG("    ?? bit %d isn't implemented\n", i);
                    break;
                }
        break;

    case uart_reg_linecontrol: // 0x0c
        s->lcr = val;
        DEBUG("    %d bits per character\n", (val & 3) + 5);
        break;

    default:;
        DEBUG("    ignored write\n");
        break;
    }
}
