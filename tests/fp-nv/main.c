//******************************************************************************
// Copyright (C) 2019, Esperanto Technologies Inc.
// The copyright to the computer program(s) herein is the
// property of Esperanto Technologies, Inc. All Rights Reserved.
// The program(s) may be used and/or copied only with
// the written permission of Esperanto Technologies and
// in accordance with the terms and conditions stipulated in the
// agreement/contract under which the program(s) have been supplied.
//------------------------------------------------------------------------------

#include <stdint.h>

int main(int, char **);

asm("        .section .text.startup,\"ax\",@progbits\n"
    "        .globl  _start\n"
    "        .type   _start, @function\n"
    "_start:\n"

    "        li      a1,1<<13\n"
    "        csrr    a0,mstatus\n"
    "        or      a0,a0,a1\n"
    "        csrw    mstatus,a0\n"

    "        la      sp,end + 65536\n"
    "        li      a1,0\n"
    "        li      a0,0\n"
    "        jal     main\n"

    "        lui     a0,0x1feed\n"
    "        csrw    0x8d0,a0\n"

    "        .size   _start, .-_start\n");

#define read_csr(reg) ({ unsigned long __tmp;       \
            asm volatile ("csrr %0, " #reg : "=r"(__tmp));      \
            __tmp; })

#define write_csr(reg, val) ({                                  \
            asm volatile ("csrw " #reg ", %0" :: "rK"(val)); })

static void     write_fflags(uint64_t v) { write_csr(fflags, v); }
static uint64_t read_fflags() { return read_csr(fflags); }

#define noinline __attribute__((noinline))

/*
 * We build up a collection of fs_madd() etc functions to test all the
 * math functions and return the flags affected.  The register
 * specifiers (eg., "rK" above) aren't defined nor guaranteed by the
 * compiler, thus by using `noinline` we are forcing this code to go
 * through the function call and thus use the ABI which _is_ defined
 * and places the arguments in known locations.
 */

#define mk_fsma_op(op)                                                  \
    static uint64_t noinline fs_##op(const double a, const double b, const double c) { \
        write_fflags(0);                                                \
        asm volatile ("        f" #op ".s fa3, fa0, fa1, fa2");         \
        return read_fflags();}

#define mk_fdma_op(op)                                                  \
    static uint64_t noinline fd_##op(const double a, const double b, const double c) { \
        write_fflags(0);                                                \
        asm volatile ("        f" #op ".d fa3, fa0, fa1, fa2");         \
        return read_fflags();}

mk_fsma_op(madd)
mk_fsma_op(msub)
mk_fsma_op(nmadd)
mk_fsma_op(nmsub)
mk_fdma_op(madd)
mk_fdma_op(msub)
mk_fdma_op(nmadd)
mk_fdma_op(nmsub)

typedef uint64_t u64;

#define fs_e_bias ((1 << (8-1)) - 1) // == 127
#define fs_e_max  ((1 << 8)     - 1) // == 255

#define fd_e_bias ((1 << (11-1)) - 1) // 1023
#define fd_e_max  ((1 << 11)     - 1) // 2047

#define fs(s1,e8,m23)  ((u64)(s1) << 31 | (u64)(e8)  << 23 | (u64)(m23))
#define fd(s1,e11,m52) ((u64)(s1) << 63 | (u64)(e11) << 52 | (u64)(m52))

#define fs_inf          fs(0,fs_e_max,0)        // s.11111111.00..0 => Inf
#define fs_snan         fs(0,fs_e_max,1)        // s.11111111.0x..x => SNAN, if x..x != 0
#define fs_qnan		fs(0,fs_e_max,1ull<<22) // s.11111111.1x..x => QNANs

#define fd_inf          fs(0,fd_e_max,0)
#define fd_snan		fs(0,fd_e_max,1)        // There are many SNANs
#define fd_qnan		fs(0,fd_e_max,1ull<<51) // There are many QNANs

#define fs_neg(v)       (fs(1,0,0) ^ (v))
#define fd_neg(v)       (fd(1,0,0) ^ (v))

#define N 16

#define NAN_BOX 0xffffffff00000000ull

uint64_t fs_magic_raw[N] = {
    0xffffffffffffffffull,  // QNAN
    0xffffffff00000000ull,  // Nan-boxed 0
    0xffffffff7fc00000ull,
    0xffffffff7f800000ull,
    0x5b9e03fa9a822823ull,
    0x7562e7fde769f8b4ull,
    NAN_BOX | 0,
    NAN_BOX | fs(0,fs_e_bias,0), // 1.0
    NAN_BOX | fs_qnan,
    NAN_BOX | fs_snan,
    NAN_BOX | fs_inf };

uint64_t fd_magic_raw[N] = {
    1,2,3,4,5,6,
    0,
    fd(0,fd_e_bias,0), // 1.0
    fd_qnan,
    fd_snan,
    fd_inf };

uint64_t global_sum = 0; // Just to keep compiler from dropping the value

int main(int c, char **v) {
    double * restrict fs_magic = (double *) fs_magic_raw;
    double * restrict fd_magic = (double *) fd_magic_raw;
    uint64_t sum = 0;

    // Turn on FP
    if (((read_csr(mstatus) >> 13) & 3) == 0)
        write_csr(mstatus,
                  read_csr(mstatus) | (1 << 13));

    /*
      One repro case:
3 0x0000008000000128 (0x22840653) f12 0xffffffffffffffff fsgnj.d fa2, fs0, fs0 0  QNAN
3 0x000000800000012c (0x229485d3) f11 0xffffffff7f800000 fsgnj.d fa1, fs1, fs1 3  Inf
3 0x0000008000000130 (0x23290553) f10 0xffffffff00000000 fsgnj.d fa0, fs2, fs2 1  0.0
    */

    sum += fs_madd(fs_magic[1], fs_magic[3], fs_magic[0]);

    for (int i = 0; i < N/2; ++i)
        fs_magic_raw[i + N/2] = NAN_BOX | fs_neg(fs_magic_raw[i]);

    for (int i = 0; i < N/2; ++i)
        fd_magic_raw[i + N/2] = fd_neg(fd_magic_raw[i]);

    for (int i = 0; i < N; ++i) {
        double fi = fs_magic[i];
        for (int j = 0; j < N; ++j) {
            double fj = fs_magic[j];
            for (int k = 0; k < N; ++k) {
                double fk = fs_magic[k];

                sum += fs_madd (fi, fj, fk);
                sum += fs_msub (fi, fj, fk);
                sum += fs_nmadd(fi, fj, fk);
                sum += fs_nmsub(fi, fj, fk); }}}

    for (int i = 0; i < N; ++i) {
        double fi = fd_magic[i];
        for (int j = 0; j < N; ++j) {
            double fj = fd_magic[j];
            for (int k = 0; k < N; ++k) {
                double fk = fd_magic[k];

                sum += fd_madd (fi, fj, fk);
                sum += fd_msub (fi, fj, fk);
                sum += fd_nmadd(fi, fj, fk);
                sum += fd_nmsub(fi, fj, fk); }}}

        global_sum = sum;

        return 0;}
