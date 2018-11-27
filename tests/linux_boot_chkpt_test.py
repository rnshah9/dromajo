#******************************************************************************
# Copyright (C) 2018, Esperanto Technologies Inc.
# The copyright to the computer program(s) herein is the
# property of Esperanto Technologies, Inc. All Rights Reserved.
# The program(s) may be used and/or copied only with
# the written permission of Esperanto Technologies and
# in accordance with the terms and conditions stipulated in the
# agreement/contract under which the program(s) have been supplied.
#------------------------------------------------------------------------------

#!/usr/bin/env python3

import unittest
import pytest
import os
import subprocess
import sys

# Do not allow generating binary artifacts
sys.dont_write_bytecode = True

# Boot linux drop a checkpoint and restart from the checkpoint
def test_linux_checkpoint_create(output_dir, riscvemu):
    linux_cfg = os.path.join(output_dir, 'bbl-vmlinux0.cfg')
    chpt_filename = os.path.join(output_dir, 'linux-checkpoint.snap')
    checkpoint_cmd = [riscvemu, '--save', chpt_filename,
                      '--maxinsns', '150000000', linux_cfg]
    print(" ".join(checkpoint_cmd), file=sys.stderr)
    subprocess.run(checkpoint_cmd, stdout=sys.stdout, stderr=sys.stderr,
                   check=True, timeout=600)
    restore_cmd = [riscvemu, '--load', chpt_filename, linux_cfg,
                   '--terminate-event', 'linux-boot']
    print(" ".join(restore_cmd), file=sys.stderr)
    subprocess.check_call(restore_cmd, timeout=600)
