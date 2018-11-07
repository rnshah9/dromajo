#!/usr/bin/env python3

import unittest
import pytest
import os
import subprocess
import sys

# Do not allow generating binary artifacts
sys.dont_write_bytecode = True

# Boot linux and expect that the execution terminates when we hit
# the write to validation CSR1
def test_linux_boot(output_dir, riscvemu):
    linux_cfg = os.path.join(output_dir, 'bbl-vmlinux0.cfg')
    cmd = [riscvemu, linux_cfg, '--terminate-event', 'linux-boot']
    print(" ".join(cmd))
    subprocess.check_call(cmd, timeout=600)
