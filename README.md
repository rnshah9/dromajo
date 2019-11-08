
# dromajo - Esperanto Technology's RISC-V Reference Model

dromajo is based on Fabrice Bellard's emulator named RISCVEMU,
since renamed `TinyEmu' [1].  Esperanto Technology has substantially modified
this to suit our needs as a golden model for co-simulation verification.

## Building

```
make -C src
```

The resulting artifacts are the `dromajo` simulator and the
`libdromajo_cosim.a` library with associated `dromajo_cosim.h`
header file.

Check the [setup.md](run/setup.md) for instructions how to compile tests like
booting Linux and baremetal for dromajo.

## Usage

The co-simulation environment will link with the libraries and usage
will depend on that, but the `src/dromajo.c` utility allows for standalone
simulation of RISC-V ELF binaries.

```
cd src
./dromajo
error: missing config file
usage: ./dromajo [--load snapshot_name] [--save snapshot_name] [--maxinsns N] [--memory_size MB] config
       --load resumes a previously saved snapshot
       --save saves a snapshot upon exit
       --maxinsns terminates execution after a number of instructions
       --terminate-event name of the validate event to terminate execution
       --trace start trace dump after a number of instructions
       --memory_size sets the memory size in MiB (default 256 MiB)

./dromajo path/to/your/coremark.riscv
...
```
