#!/usr/bin/env python3
"""Check a small shared fabric-latency sweep against all measured cases."""
import json
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess

repo = Path(__file__).resolve().parents[2]
out = repo/'build/sparse_dma'
base = json.loads((repo/'scripts/sparse_dma/config.json').read_text())
references = json.loads((out/'rtl-results.json').read_text())['results']
validation = json.loads((out/'validation/manifest.json').read_text())
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', default='parameter_sweep',
                    help='New evidence directory name under build/sparse_dma')
args = parser.parse_args()
if '/' in args.output or args.output.startswith('.'):
    parser.error('--output must be a simple directory name')


def measure(log, rtl=False):
    text = log.read_text()
    cycles = int(re.search(r'The gather took (\d+) cycles', text).group(1))
    if rtl:
        starts = [float(x) for x in re.findall(r'\[DMA\] ([\d.]+) ns issue', text)]
        ends = [float(x) for x in re.findall(r'\[DMADONE\] ([\d.]+) ns', text)]
    else:
        starts = [int(x) for x in re.findall(r'DMA_BEGIN id=\d+ cycle=(\d+)', text)]
        ends = [int(x) for x in re.findall(r'DMA_END id=\d+ cycle=(\d+)', text)]
    if ('CORRECT!' not in text or 'WRONG!' in text
            or not re.search(r'\[EOC\].*retval\s*=\s*0\b', text)
            or (log.parent/'exit-status.txt').read_text().strip() != '0'):
        raise RuntimeError(f'Incorrect data or failed termination: {log}')
    return cycles, max(ends)-starts[1]


def compare():
    rows = []
    for ref in references:
        actual = measure(out/'runs'/ref['variant']/'simulation.log')
        expected = ref['cycles'], ref['dma_window_ns']
        rows.append(dict(case='original/'+ref['variant'], rtl=expected, gvsoc=actual))
    for case in validation:
        case_path = out/'validation'/case['case']
        rows.append(dict(case=case['case']+'/'+case['variant'],
            rtl=measure(case_path/'rtl'/case['variant']/'simulation.log', True),
            gvsoc=measure(case_path/'gvsoc'/case['variant']/'simulation.log')))
    for row in rows:
        row['errors_pct'] = [100*(a/b-1) for a,b in zip(row['gvsoc'], row['rtl'])]
    return rows


sweep = out/args.output
sweep.mkdir(exist_ok=False)
shutil.copy2(repo/'pulp/targets/spatz_sparse_dma.py', sweep/'spatz_sparse_dma.py')
shutil.copy2(repo/'pulp/pulp/snitch/sparse_dma_icache_prefetch.cpp',
             sweep/'sparse_dma_icache_prefetch.cpp')
summary = []
for latency in dict.fromkeys((base['axi_latency'], 6, 4, 10, 12)):
    dest = sweep/f'axi-{latency}'
    dest.mkdir(exist_ok=False)
    config = dict(base, axi_latency=latency)
    (dest/'config.json').write_text(json.dumps(config, indent=2)+'\n')
    if latency == base['axi_latency']:
        run_configs = [out/'runs'/r['variant']/'config.json' for r in references]
        run_configs += [out/'validation'/r['case']/'gvsoc'/r['variant']/'config.json'
                        for r in validation]
        if any(json.loads(path.read_text()) != config for path in run_configs):
            raise RuntimeError('Run all cases with the current configuration before calibrating.')
    if latency != base['axi_latency']:
        env = dict(os.environ, SPARSE_DMA_CONFIG=str(dest/'config.json'))
        print(f'Checking shared AXI latency {latency} across 12 cases...', flush=True)
        with (dest/'driver.log').open('w') as stream:
            for variant in ('gather', 'gather-opt', 'gather-hw'):
                subprocess.run(['bash', 'scripts/sparse_dma/step.sh', 'run', variant],
                               cwd=repo, env=env, stdout=stream, stderr=subprocess.STDOUT, check=True)
            subprocess.run(['python3', 'scripts/sparse_dma/run_validation.py', 'gvsoc'],
                           cwd=repo, env=env, stdout=stream, stderr=subprocess.STDOUT, check=True)
    rows = compare()
    worst = max(abs(error) for row in rows for error in row['errors_pct'])
    summary.append(dict(axi_latency=latency, worst_error_pct=worst, results=rows))
    (dest/'results.json').write_text(json.dumps(summary[-1], indent=2)+'\n')
    shutil.copytree(out/'runs', dest/'runs')
    for name in dict.fromkeys(c['case'] for c in validation):
        shutil.copytree(out/'validation'/name/'gvsoc', dest/'validation'/name)
    (sweep/'results.json').write_text(json.dumps(summary, indent=2)+'\n')
    print(f'AXI latency {latency}: worst error {worst:.2f}%', flush=True)
    if worst <= 10:
        shutil.copy2(dest/'config.json', repo/'scripts/sparse_dma/config.json')
        print(f'Selected shared AXI latency {latency}; all 24 timing measurements pass.', flush=True)
        break
else:
    raise SystemExit(f'No sweep point meets all 10% checks; inspect {sweep}/results.json')
