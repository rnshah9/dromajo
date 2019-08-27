# RISCVEMU Memory Map

The memory map depends in part on the configuration, eg. the MMIO
range is given in the configuration file, but the following are the
defaults.

    00_0000_0000   00_0000_0FFF     Dummy memory for uaccess-etcsr
    00_0001_0000   00_0001_0FFF     Boot ROM

    00_0200_0000   00_020b_FFFF     CLINT
    00_1000_0000   00_11FF_FFFF     PLIC
    00_1200_2000   00_1200_2FFF     Synopsys DesignWare ABP UART0
    00_1200_7000   00_1200_7FFF     Synopsys DesignWare ABP UART1
    00_5400_0000   00_5400_001F     SiFive UART, disabled by default

    80_0000_0000   80_0FFF_FFFF     Global memory, defaults to 256 MiB
