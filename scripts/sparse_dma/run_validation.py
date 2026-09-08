#!/usr/bin/env python3
"""Run identical validation ELF files in RTL or GVSoC, preserving evidence."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('simulator', choices=('rtl', 'gvsoc'))
parser.add_argument('--case')
parser.add_argument('--variant')
parser.add_argument('--trace-core', action='store_true')
args = parser.parse_args()
repo = Path(__file__).resolve().parents[2]
out = repo / 'build/sparse_dma/validation'
rtl = repo.parent / 'spatz-sparse-dma/hw/system/spatz_cluster'
manifest = json.loads((out / 'manifest.json').read_text())
failures = []
for item in manifest:
    if args.case and args.case != item['case']:
        continue
    if args.variant and args.variant != item['variant']:
        continue
    binary = out / item['binary']
    if hashlib.sha256(binary.read_bytes()).hexdigest() != item['sha256']:
        raise RuntimeError(f'Binary changed after build: {binary}')
    run = out / item['case'] / ('profile' if args.trace_core else args.simulator) / item['variant']
    run.mkdir(parents=True, exist_ok=True)
    if args.simulator == 'rtl':
        command = [str(rtl / 'bin/spatz_cluster.vsim'), str(binary)]
        cwd = rtl  # RTL launcher and trace filenames require this directory.
    else:
        command = [str(repo / 'install/bin/gvsoc'), '--target-dir=' + str(repo / 'pulp/targets'),
            '--target=spatz_sparse_dma', '--binary=' + str(binary), 'run',
            '--trace=idma', '--trace-level=trace']
        if args.trace_core:
            command += ['--trace=pe0/insn', '--trace=pe0/lsu', '--trace=l1_icache']
        cwd = run
        shutil.copy2(os.environ['SPARSE_DMA_CONFIG'], run / 'config.json')
    started = time.monotonic()
    with (run / 'simulation.log').open('w') as stream:
        try:
            status = subprocess.run(command, cwd=cwd, stdout=stream, stderr=subprocess.STDOUT,
                                    timeout=1800 if args.simulator == 'rtl' else 300).returncode
        except subprocess.TimeoutExpired:
            status = 124
    (run / 'exit-status.txt').write_text(str(status) + '\n')
    log = (run / 'simulation.log').read_text()
    if args.trace_core:
        # UART byte stores and instruction traces share stdout. Reconstruct
        # the UART text by removing complete timestamped trace records.
        log = re.sub(r'\x1b\[[0-9;]*m', '', log)
        def remove_trace(match):
            # A printed cycle-count digit may immediately precede the
            # timestamp. At 1 GHz the timestamp in ps is cycle*1000 plus
            # a fractional-cycle remainder; retain any leading UART digits.
            stamp, cycle = match.group(1), int(match.group(2))
            for prefix in range(len(stamp)):
                if int(stamp[prefix:]) // 1000 == cycle:
                    return stamp[:prefix]
            return ''
        log = re.sub(r'(\d+):\s+(\d+):\s+\[[^\n]*\][^\n]*\n', remove_trace, log)
    passed = status == 0 and 'CORRECT!' in log and not re.search(
        r'WRONG!|Mismatch gather|\*\* Error|\*\* Fatal|Invalid access', log)
    cycle_match = re.search(r'The gather took (\d+) cycles', log)
    cycles = int(cycle_match.group(1)) if cycle_match else None
    passed = passed and cycles is not None and bool(re.search(
        r'\[EOC\].*retval\s*=\s*0\b', log))
    evidence = dict(item, simulator=args.simulator, command=command, cwd=str(cwd),
                    status=status, correct=passed, cycles=cycles,
                    wall_seconds=time.monotonic()-started)
    (run / 'run.json').write_text(json.dumps(evidence, indent=2) + '\n')
    if args.simulator == 'rtl':
        for database in rtl.glob('DRAMSysRecordable0_*.tdb'):
            shutil.copy2(database, run / database.name)
    print(f"{args.simulator} {item['case']}/{item['variant']}: "
          f"cycles={cycles}, correct={passed}, wall={evidence['wall_seconds']:.1f}s", flush=True)
    if not passed:
        failures.append(str(run))
if failures:
    raise SystemExit('Failed runs: ' + ', '.join(failures))
