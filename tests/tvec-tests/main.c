//******************************************************************************
// Copyright (C) 2019, Esperanto Technologies Inc.
// The copyright to the computer program(s) herein is the
// property of Esperanto Technologies, Inc. All Rights Reserved.
// The program(s) may be used and/or copied only with
// the written permission of Esperanto Technologies and
// in accordance with the terms and conditions stipulated in the
// agreement/contract under which the program(s) have been supplied.
//------------------------------------------------------------------------------

/*

  Try writing some values to [sm]tvec and do an ecall.  This is for
  cosimulation checks.
*/

asm("        .section .text.startup,\"ax\",@progbits\n"
    "        .globl  _start\n"
    "        .type   _start, @function\n"
    "_start:\n"
    "        li	     a0, -1\n"
    "        csrw    mtvec, a0\n"
    "        csrr    a1, mtvec\n"
    "        csrw    stvec, a0\n"
    "        csrr    a1, stvec\n"

    "        li	     a0, -2\n"
    "        csrw    mtvec, a0\n"
    "        csrr    a1, mtvec\n"
    "        csrw    stvec, a0\n"
    "        csrr    a1, stvec\n"

    "        lui     a0,0x1feed\n"
    "        csrw    0x8d0,a0\n"

    "        .size   _start, .-_start\n");
