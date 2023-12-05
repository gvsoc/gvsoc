#!/usr/bin/env python3

import pexpect
import random
import os
import sys


while True:

    port = random.randint(4000, 20000)

    os.environ['GV_PROXY_PORT'] = str(port)

    try:
        run = pexpect.spawn(f'make run runner_args="--config-opt=**/gvsoc/proxy/enabled=true --config-opt=**/gvsoc/proxy/port={port}"', encoding='utf-8', logfile=sys.stdout, env=os.environ)
        match = run.expect(['Opened proxy on socket '], timeout=None)
        if match == 0:
            break

        run.expect(pexpect.EOF, timeout=None)
    except:
        pass

proxy = pexpect.spawn(f'./gvcontrol --host=localhost --port={port}', encoding='utf-8', logfile=sys.stdout)

while True:
    match = run.expect(['Hello', pexpect.EOF, pexpect.TIMEOUT], timeout=1)
    if match == 0:
        break
    elif match == 1:
        exit(1)

    proxy.expect([pexpect.EOF, pexpect.TIMEOUT], timeout=1)


exit(run.exitstatus)