//******************************************************************************
// Copyright (C) 2019, Esperanto Technologies Inc.
// The copyright to the computer program(s) herein is the
// property of Esperanto Technologies, Inc. All Rights Reserved.
// The program(s) may be used and/or copied only with
// the written permission of Esperanto Technologies and
// in accordance with the terms and conditions stipulated in the
// agreement/contract under which the program(s) have been supplied.
//------------------------------------------------------------------------------
//
// This code is based on dv/tests/ioshire/uart_bdrt_div_val/spMin/uart_bdrt_div_val.c

#include "DW_common.h"
#include "DW_apb_uart_public.h"
#include "DW_apb_uart_private.h"
#include <string.h>

#define TIMEOUT 100

// Also, see https://www.mjmwired.net/kernel/Documentation/devicetree/bindings/serial/snps-dw-apb-uart.txt
// https://github.com/torvalds/linux/blob/master/Documentation/devicetree/bindings/serial/snps-dw-apb-uart.yaml

int main(int, char **);

asm("        .section .text.startup,\"ax\",@progbits\n"
    "        .globl  _start\n"
    "        .type   _start, @function\n"
    "_start: la      sp,end + 65536\n"
    "        li      a1,0\n"
    "        li      a0,0\n"
    "        j       main\n"
    "        .size   _start, .-_start\n");

void printDbg(const char *fmt, ...)
{
    // We can't dump anything as these whole point of this test is to test IO
}

void onAssert__(char const *file, unsigned line)
{
    printDbg( "File: %s   line: %d", file, line );
    for (;;);
}

static void usleep(int us)
{
    // Sleep for us 1e-6 seconds.  Assuming 1.5e9 Hz clock frequency and an IPC < 3
    for (int timeout = 1500 * us; 0 <= timeout; --timeout)
        asm volatile("" :: "r" (timeout)); // Non-removable instruction is essential
}

#define ET_UART0_BASE_ADDR      0x12002000

int main(int argc, char *argv[])
{
    static const int        freq = 25000000;
    static const int        bps  =   115200; // Not "baud", cf. http://tldp.org/HOWTO/Modem-HOWTO-23.html
    static const int        div  = freq / (16 * bps);
    struct dw_list_head     uartListHead;
    struct dw_uart_param    uartParams;
    struct dw_uart_instance uartInstance;
    struct dw_device        et_uart = {
        .name           = "snps UART0 Driver Device",
        .data_width     = 32,
        .comp_type      = Dw_apb_uart,
        .comp_param     = (void *)&uartParams,
        .instance       = (void *)&uartInstance,
        .list           = uartListHead,
        .base_address   = (void *)ET_UART0_BASE_ADDR,
    };

    dw_uart_init(&et_uart);
    dw_uart_setClockDivisor(&et_uart, div);

    // Have to wait 32 * div cycles (@ freq), IOW
    // 32 * freq / 16 / bps @ freq/s = (2 * freq / bps) * 1/freq = 2/bps sec
    //usleep(2000000 / bps);  XXX For simulations this takes too long
    usleep(0);

    if (dw_uart_getClockDivisor(&et_uart) != div)
        onAssert__(__FILE__, __LINE__);

    dw_uart_setLineControl(&et_uart, Uart_line_8n1);
    dw_uart_enableFifos(&et_uart);

    // disable programmable THR empty mode
    dw_uart_disablePtime(&et_uart);

    static const char *transmit_string = "Hello Esperanto!";
    char receive_buf[40], *receive_string = receive_buf;

    int timeout = TIMEOUT;
    while (receive_string < receive_buf + sizeof receive_buf - 1 && timeout--) {
        enum dw_uart_line_status lineStatus = dw_uart_getLineStatus(&et_uart);

        // check for an error or break condition on uart
        if ((lineStatus & (Uart_line_oe | Uart_line_pe | Uart_line_fe |
                           Uart_line_bi | Uart_line_rfe)) != 0) {
            printDbg("[info] device uart is busy::\n");
            printDbg("Line Status: 0x%02x\n", lineStatus);
        }

        if (lineStatus & Uart_line_dr) {
            *receive_string++ = dw_uart_read(&et_uart);
            timeout = TIMEOUT;
        }

        if ((lineStatus & Uart_line_thre) != 0 && *transmit_string) {
            dw_uart_write(&et_uart, *transmit_string++);
            timeout = TIMEOUT;
        }
    }

    *receive_string++ = 0;

    if (timeout <= 0)
        printDbg("\n[info] timed out\n");

    printDbg("Received %s\n", receive_buf);

    return 0;
}

char *strncpy(char *__restrict dest, const char *__restrict src, size_t n)
{
    size_t i;

    for (i = 0; i < n && src[i] != '\0'; i++)
        dest[i] = src[i];

    for (; i < n; i++)
        dest[i] = '\0';

    return dest;
}

void *memcpy(void *__restrict dest, const void *__restrict src, size_t n)
{
    uint8_t       *d = dest;
    const uint8_t *s = src;

    while (n--)
        *d++ = *s++;
}
