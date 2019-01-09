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
