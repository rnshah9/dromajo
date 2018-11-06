#!/usr/bin/env python3

import pexpect
import os
import sys

# Do not allow generating binary artifacts
sys.dont_write_bytecode = True

# Test reaching the linux boot
def test_linux_boot(output_dir, riscvemu):
    linux_cfg = os.path.join(output_dir, 'bbl-vmlinux0.cfg')
    cmd = [riscvemu, linux_cfg]
    child = pexpect.spawn(" ".join(cmd), encoding='utf-8', cwd=output_dir)
    child.logfile = sys.stdout
    child.expect(".*LINUX BOOT DONE.*", timeout=600)
    child.close()
