RISCVEMU 20190319 Release Notes
===============================

End-user visible Changes since 20190121

   * Sped excution by ~ 8 X

   * Memory start address can now be set with either
     "memory_base_addr: 0x8000000000," in the .cfg or using the
     --memory_addr=0x8000000000 command line option.

     We currently only support 0x80000000 (2 GiB) and 0x8000000000
     (512 GiB, the SoC address).

   * Support for ET CSRs PREFETCH, FLUSHVAR, and FLUSHVAW were added

     The latter two can raise MMU exceptions, violating RISC-V spec
     which we had to support as well.

   * MCE stubs CSRs (0xBA0 - BA4) were added

   * MCAUSE have more bits than SCAUSE

     There are undelegatable bits interrupts which explains why SCAUSE
     have fewer bits.

   * Physical address size is configurable and truncate MTVEC accordingly

     Get physical address bits from config file "physical_addr_len"
     parameter which defaults to 40.

   * Intersepts SBI SHUTDOWN ecall and terminate simulation, unless
     --ignore_sbi_shutdown is given or XXX config parameter is used.

   * mtime can now be set

   * Support for PMP was added (only 8 regions)

   * Termination of HTIF's tohost should be more robust

   * Debug mode is now supported, but is currently used internally
     and turned off as part of the boot rom.

Many internal and co-simulation related changes omitted.


RISCVEMU 20190121 Release Notes
===============================

End-user visible Changes since v0.2

   * c.add x0, ... is a hint, not an illegal instruction

   * Hardwired UXL/SXL to only support RV64

   * Relocate PLIC to 0xC000000

   * COUNTEREN fixes

     For reasons lost to history we were masking off too many bits.
     Also, mcounteren was affected user mode access (it shouldn't)

   * Enforce 64-byte alignment for vectored interrupts

   * Match DUT & Spike on melegeg

   * CSR flushall-et can also be read now


RISCVEMU v0.2 Release Notes
===========================

End-user visible Changes since v0.1

   * Fixed many instaces where simulator would assert() or exit() out
     instead of properly propagating termination

   * Partial (dummy) support for the Esperanto Flush CSR (0x81F)

   * Some illegal instruction weren't caught:

       - c.lui with immediate == 0
       - c.addiw with rd == 0
       - c.lqsp with rd == 0
       - c.lwsp with rd == 0
       - c.ldsp with rd == 0
       - c.add with rd == 0
       - jalr with non-zero funct3

   * A some hpmcounter could be written from user mode

   * mvendor CSR now fixed to Esperanto JEDEC number 101 in bank 11, that
     is 0x5e5


RISCVEMU v0.1 Release Notes
===========================

RISCVEMU is based upon Fabrice Bellard's 2017-08-06 tarball drop, with
substantial changes and bugfixes and brought up-to-date with the
latest in-progress version of the RISC-V specifications.  The changes
are too numerous to list.
