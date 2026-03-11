#!/usr/bin/env python3

import pexpect
import os
import sys


run = pexpect.spawn("gvsoc --target=rv64_untimed --binary ./spike_fw_payload.elf  run", encoding='utf-8', logfile=sys.stdout, env=os.environ)
match = run.expect(['NFS preparation skipped, OK'], timeout=None)
match = run.expect(['#'], timeout=None)