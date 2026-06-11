#!/usr/bin/env python3

import pexpect
import os
import sys


# The binary path must be absolute since gvrun runs from its work directory
binary = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'spike_fw_payload.elf')

run = pexpect.spawn(f"gvrun --target rv64_untimed --param soc/binary={binary} run", encoding='utf-8', logfile=sys.stdout, env=os.environ)
match = run.expect(['NFS preparation skipped, OK'], timeout=None)
match = run.expect(['#'], timeout=None)