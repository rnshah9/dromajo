#!/usr/bin/env python3

import unittest
import pexpect
import pytest
import os
import subprocess
import sys


# Do not allow generating binary artifacts
sys.dont_write_bytecode = True

def test_checkpoint_create(output_dir, riscvemu):
    print(output_dir, riscvemu)
    chpt_filename = os.path.join(output_dir, 'simple.snap')
    test_cfg = os.path.join(output_dir, 'rv64ui-p-simple.cfg')
    checkpoint_cmd = [riscvemu, '--save', chpt_filename, '--trace', '0',
                      '--maxinsns', '10', test_cfg]
    print(" ".join(checkpoint_cmd))
    subprocess.run(checkpoint_cmd, stdout=sys.stdout, stderr=sys.stderr,
                   check=True, timeout=60)
    restore_cmd = [riscvemu, '--load', chpt_filename, test_cfg, '--trace', '0']
    print(" ".join(restore_cmd))
    child = pexpect.spawn(" ".join(restore_cmd), encoding='utf-8')
    child.logfile = sys.stdout
    child.expect("3 0x0000000080000044 \(0xfc3f2023\)", timeout=60)
    child.close()
