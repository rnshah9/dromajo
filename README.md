//******************************************************************************
// Copyright (C) 2018,2019, Esperanto Technologies Inc.
// The copyright to the computer program(s) herein is the
// property of Esperanto Technologies, Inc. All Rights Reserved.
// The program(s) may be used and/or copied only with
// the written permission of Esperanto Technologies and
// in accordance with the terms and conditions stipulated in the
// agreement/contract under which the program(s) have been supplied.
//------------------------------------------------------------------------------

# RISCVEMU - Esperanto Technology's RISC-V Reference Model

RISCVEMU is based on Fabrice Bellard's emulator of the same name,
since renamed `TinyEmu' [1].  We have substantially modified this to
suit our needs as a golden model for co-simulation with ET-Maxion.

## Building

```
$ make
gcc -O2 -Wall -std=gnu99 -g -Werror -Wno-parentheses -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -MMD -D_GNU_SOURCE -DCONFIG_VERSION=\"2017-08-06\" -DVERIFICATION -DCONFIG_SLIRP -DMAX_XLEN=32 -c -o riscv_cpu32.o riscv_cpu.c
gcc -O2 -Wall -std=gnu99 -g -Werror -Wno-parentheses -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -MMD -D_GNU_SOURCE -DCONFIG_VERSION=\"2017-08-06\" -DVERIFICATION -DCONFIG_SLIRP -c -o virtio.o virtio.c
....
```

(We recommend setting the `MAKEFLAGS` environment variable to
`-j$(nproc)` for faster builds.)

The resulting artifacts are the `riscvemu` simulator and the
`libriscvemu_cosim.a` library with associated `riscvemu_cosim.h`
header file.

## Usage

The co-simulation environment will link with the libraries and usage
will depend on that, but the `vharness` utility allows for standalone
simulation of RISC-V ELF binaries.

```
$ ./riscvemu
error: missing config file
usage: ./riscvemu [--load snapshot_name] [--save snapshot_name] [--maxinsns N] [--memory_size MB] config
       --load resumes a previously saved snapshot
       --save saves a snapshot upon exit
       --maxinsns terminates execution after a number of instructions
       --terminate-event name of the validate event to terminate execution
       --trace start trace dump after a number of instructions
       --memory_size sets the memory size in MiB (default 256 MiB)

$ ./riscvemu coremark.riscv 2> coremark.trace
fstat not supported by fakeos
coremark with hardcoded args
ERROR! Main has no argc, but SEED_METHOD defined to SEED_ARG!
3.coremark with hardcoded args
...
```
