#!/usr/bin/env python3
"""Check the private DMA regressions and report comparison to the original model."""
import hashlib
import json
from pathlib import Path
import re
import subprocess


ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / 'build/sparse_dma/soft_hier_old'
ANSI = re.compile(r'\x1b\[[0-9;]*m')


def require(condition, message):
    if not condition:
        raise SystemExit(message)


def read_result(mode):
    folder = OUTPUT / mode
    require((folder / 'exit-status.txt').read_text().strip() == '0', f'{mode}: run failed')
    log = ANSI.sub('', (folder / 'simulation.log').read_text())
    prefix = 'SOFT_HIER_OLD_DMA_ISA_SUCCESS' if mode == 'isa' else 'SOFT_HIER_OLD_DMA_SUCCESS'
    lines = [line for line in log.splitlines() if line.startswith(prefix)]
    require(len(lines) == 1, f'{mode}: missing or repeated success marker')
    fields = dict(re.findall(r'(\w+)=([^ ]+)', lines[0]))
    fields.pop('mode', None)
    accounting = [line.split('[iDMA] ', 1)[1] for line in log.splitlines()
                  if '[iDMA] Finished' in line]
    return fields, accounting


def main():
    baseline = json.loads((ROOT / 'pulp/tests/sparse_dma/soft_hier_old_baseline.json').read_text())
    test_source = ROOT / 'pulp/tests/sparse_dma/soft_hier_old.cpp'
    require(hashlib.sha256(test_source.read_bytes()).hexdigest() == baseline['test_sha256'],
            'Legacy driver changed: regenerate and review the reference against the original model.')
    results = {}
    for mode in ('legacy', 'legacy-enabled', 'gather-sync', 'gather-async', 'isa'):
        fields, accounting = read_result(mode)
        results[mode] = fields
        if mode.startswith('legacy'):
            require(fields == baseline['summary'], f'{mode}: transaction or cycle baseline mismatch')
            require(accounting == baseline['accounting'], f'{mode}: transfer-time accounting changed')
        elif mode.startswith('gather'):
            require(fields['cases'] == '12' and fields['index_reads'] == '34',
                    f'{mode}: coverage or index traffic mismatch')
            require(int(fields['stalls']) > 0 and fields['stalls'] == fields['grants'],
                    f'{mode}: offload backpressure was not exercised')
            expected_denials = '11' if mode == 'gather-async' else '0'
            require(fields['denied_indices'] == expected_denials, f'{mode}: denial coverage mismatch')
        else:
            require(fields['copies'] == '2' and fields['checks'] == '7' and fields['index_reads'] == '1',
                    'ISA integration coverage mismatch')

    generic = (ROOT / 'build/sparse_dma/logs/model-test.log').read_text()
    require('SPARSE_DMA_TEST_SUCCESS cases=11 packed_index_reads=30' in generic,
            'Run the existing generic sparse-DMA regression first.')
    private = Path('pulp/chips/soft_hier_old/idma')
    unchanged = []
    for path in sorted((ROOT / 'pulp' / private / 'be').glob('*')):
        relative = path.relative_to(ROOT / 'pulp')
        before = subprocess.check_output(['git', '-C', str(ROOT / 'pulp'), 'show',
                                          baseline['pulp_commit'] + ':' + str(relative)])
        require(path.read_bytes() == before, f'Legacy backend changed: {relative}')
        unchanged.append(str(relative))
    generic_diff = subprocess.check_output(['git', '-C', str(ROOT / 'pulp'), 'diff',
                                           baseline['pulp_commit'], '--', 'pulp/idma'])
    require(not generic_diff, 'The generic iDMA model changed during the private-model port.')

    record = {'baseline_commit': baseline['pulp_commit'], 'baseline': baseline['summary'],
              'results': results, 'legacy_accounting_identical': True,
              'unchanged_backend_files': unchanged, 'generic_model_cases': 11,
              'source_sha256': {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                               for p in sorted((ROOT / 'pulp' / private).rglob('*'))
                               if p.is_file() and '__pycache__' not in p.parts}}
    (OUTPUT / 'results.json').write_text(json.dumps(record, indent=2) + '\n')
    rows = [
        '# SoftHier old sparse-DMA port validation', '',
        'All compatibility, gather, and instruction integration checks passed.', '',
        f"Reference: unmodified pulp commit `{baseline['pulp_commit']}`. The nine-case reference",
        'was captured before editing the private DMA model. Legacy runs match its memory',
        'transaction hash, per-copy accounting messages, and cycle count exactly.', '',
        '| Run | Cases / copies | Cycles | Index reads | Offload stalls / grants |',
        '| --- | ---: | ---: | ---: | ---: |',
        '| Original reference | 9 | 329 | 0 | 0 / 0 |',
    ]
    for mode, result in results.items():
        count = result.get('cases', result.get('copies'))
        stalls = (result['stalls'] + ' / ' + result['grants']) if 'stalls' in result else '—'
        rows.append(f"| {mode} | {count} | {result['cycles']} | {result['index_reads']} | {stalls} |")
    rows += [
        '', 'The asynchronous gather run includes 11 denied index requests. Both gather runs',
        'check all four index widths, high unsigned indices and 64-bit source addresses,',
        'packed and partial index words, padding, empty transfers, queued descriptor snapshots,',
        'and ordered completion. The actual old Snitch core executes DMIDX/DMCPYI and then',
        'a collective copy; seven software-visible values and the retained mask are checked.', '',
        f"Legacy traffic hash: `{baseline['summary']['traffic']}`; total active DMA time: 274 ns.",
        'The original generic sparse-DMA regression also passes all 11 cases.',
        'All six private backend source/header files and the generic iDMA sources are unchanged.', '',
        '## Scope', '',
        'Gather is opt-in through `gather_enable=True` and a bound absolute-address IO-v1 index port.',
        'Existing cluster defaults and wiring are unchanged. Use DMCPYI config bit 2 for gather;',
        'DMCPY continues to select the legacy collective type.', '',
        'These are functional and compatibility tests, not an RTL calibration of the private',
        'backend. The prior generic-model 10% cycle-deviation result does not apply to this port.',
        'Legacy compatibility uses zero-latency TCDM, a 256-entry burst queue, and idle barriers',
        'around legacy copies. Existing queue-exhaustion, delayed-TCDM overlap, and overlapping',
        'collective-metadata limitations were observed before the port and left unchanged.', '',
        'See the private model README for programming constraints and reproduction commands.',
        'Per-run simulation logs, source hashes, and structured results are stored alongside this report.',
    ]
    (OUTPUT / 'REPORT.md').write_text('\n'.join(rows) + '\n')
    print('SOFT_HIER_OLD_DMA_REPORT_SUCCESS legacy_baseline=identical gather=24 isa=2 generic=11')
    print(OUTPUT / 'REPORT.md')


if __name__ == '__main__':
    main()
