#!/usr/bin/env python3

#******************************************************************************
# Copyright (C) 2018,2019, Esperanto Technologies Inc.
# The copyright to the computer program(s) herein is the
# property of Esperanto Technologies, Inc. All Rights Reserved.
# The program(s) may be used and/or copied only with
# the written permission of Esperanto Technologies and
# in accordance with the terms and conditions stipulated in the
# agreement/contract under which the program(s) have been supplied.
#------------------------------------------------------------------------------

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
