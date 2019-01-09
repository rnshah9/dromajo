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

import pexpect
import os
import sys

# Do not allow generating binary artifacts
sys.dont_write_bytecode = True

# Test reaching the linux boot
def test_linux_boot(output_dir, riscvemu):
    linux_cfg = os.path.join(output_dir, 'bbl-vmlinux0.cfg')
    cmd = [riscvemu, linux_cfg]
    print("Command: " + " ".join(cmd))
    sys.stdout.flush()
    child = pexpect.spawn(" ".join(cmd), encoding='utf-8', cwd=output_dir)
    child.logfile = sys.stdout
    child.expect(".*LINUX BOOT DONE.*", timeout=600)
    child.close()
