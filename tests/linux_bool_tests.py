#!/usr/bin/env python3

import unittest
import pexpect
import pytest
import os
import subprocess
import sys

# Do not allow generating binary artifacts
sys.dont_write_bytecode = True

def test_linux_boot(output_dir, vharness):
    linux_cfg = os.path.join(output_dir, 'bbl-vmlinux0.cfg')
    cmd = [vharness, linux_cfg]
    print(" ".join(cmd))
    child = pexpect.spawn(" ".join(cmd), encoding='utf-8', cwd=output_dir)
    child.logfile = sys.stdout
    child.expect("Kernel panic - not syncing: VFS: Unable to mount root fs on", timeout=60)
    child.close()
