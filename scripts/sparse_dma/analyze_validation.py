#!/usr/bin/env python3
"""Measure additional RTL/GVSoC pairs with one shared model configuration."""
import csv
import json
from pathlib import Path
import re

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np


def analyze(out):
    validation = out / 'validation'
    manifest = json.loads((validation / 'manifest.json').read_text())
    results = []
    for item in manifest:
        row = dict(item)
        for simulator in ('rtl', 'gvsoc'):
            run = validation / item['case'] / simulator / item['variant']
            log = re.sub(r'\x1b\[[0-9;]*m', '', (run / 'simulation.log').read_text())
            cycles = re.search(r'The gather took (\d+) cycles', log)
            if not cycles:
                raise RuntimeError(f'Missing cycle measurement in {run}')
            row[simulator + '_cycles'] = int(cycles.group(1))
            if simulator == 'rtl':
                begins = [(int(i), float(t)) for t, i in re.findall(
                    r'\[DMA\] ([\d.]+) ns issue id=(\d+)', log)]
                ends = {int(i): float(t) for t, i in re.findall(
                    r'\[DMADONE\] ([\d.]+) ns id=(\d+)', log)}
                eoc = re.search(r'\[EOC\].*retval = 0', log)
            else:
                begins = [(int(i), int(t)) for i, t in re.findall(
                    r'DMA_BEGIN id=(\d+) cycle=(\d+)', log)]
                ends = {int(i): int(t) for i, t in re.findall(
                    r'DMA_END id=(\d+) cycle=(\d+)', log)}
                eoc = re.search(r'\[EOC\] retval=0 ', log)
            gather = begins[1:]
            expected = 1 if item['variant'] == 'gather-hw' else item['count']
            if len(gather) != expected or any(i not in ends for i, _ in gather):
                raise RuntimeError(f'Missing DMA issue/completion events in {run}')
            row[simulator + '_dma_ns'] = max(ends[i] for i, _ in gather) - gather[0][1]
            row[simulator + '_correct'] = bool(eoc and int((run / 'exit-status.txt').read_text()) == 0
                and 'CORRECT!' in log and not re.search(
                    r'WRONG!|Mismatch gather|\*\* Error|\*\* Fatal|Invalid access', log))
            row[simulator + '_log'] = str((run / 'simulation.log').relative_to(out))
        row['cycles_error_pct'] = 100 * (row['gvsoc_cycles'] / row['rtl_cycles'] - 1)
        row['dma_error_pct'] = 100 * (row['gvsoc_dma_ns'] / row['rtl_dma_ns'] - 1)
        row['within_10pct'] = row['rtl_correct'] and row['gvsoc_correct'] and max(
            abs(row['cycles_error_pct']), abs(row['dma_error_pct'])) <= 10
        results.append(row)

    (validation / 'results.json').write_text(json.dumps(dict(results=results,
        all_within_10pct=all(r['within_10pct'] for r in results)), indent=2) + '\n')
    with (validation / 'results.csv').open('w') as stream:
        writer = csv.DictWriter(stream, fieldnames=results[0].keys())
        writer.writeheader()
        writer.writerows(results)

    plots = out / 'plots'
    plots.mkdir(exist_ok=True)
    groups = list(dict.fromkeys(r['case'] for r in results))
    fig, axes = plt.subplots(1, len(groups), figsize=(12, 4.2), sharey=True)
    limit = max(14, max(abs(r[k]) for r in results for k in ('cycles_error_pct', 'dma_error_pct')) * 1.25)
    for ax, group in zip(axes, groups):
        rows = [r for r in results if r['case'] == group]
        ax.axhspan(-10, 10, color='#6da567', alpha=.16)
        for shift, key, name, color in ((-.18, 'cycles_error_pct', 'Software cycles', '#315b96'),
                                      (.18, 'dma_error_pct', 'DMA window', '#de8232')):
            bars = ax.bar(np.arange(3)+shift, [r[key] for r in rows], .34, color=color, label=name)
            ax.bar_label(bars, fmt='%+.1f%%', padding=3, fontsize=9)
        ax.axhline(0, color='black', linewidth=.6)
        ax.axhline(10, color='#6da567', linestyle='--', linewidth=.8)
        ax.axhline(-10, color='#6da567', linestyle='--', linewidth=.8)
        ax.set_xticks(np.arange(3), ['Core loop', 'Inlined loop', 'HW gather'], rotation=15)
        ax.set_title(f"G={rows[0]['count']}, {rows[0]['pattern']}, seed {rows[0]['seed']}")
        ax.set_ylim(-limit, limit)
        ax.spines[['top', 'right']].set_visible(False)
    axes[0].set_ylabel('(GVSoC − RTL) / RTL (%)')
    axes[1].legend(frameon=False, loc='lower center', fontsize=9)
    fig.suptitle('Additional workloads, using the unchanged calibration configuration')
    fig.tight_layout()
    for extension in ('png', 'svg', 'pdf'):
        fig.savefig(plots / f'validation_deviation.{extension}', dpi=150, bbox_inches='tight')
    plt.close(fig)

    rows = '\n'.join(f"| G={r['count']}, {r['pattern']} | {r['variant']} | {r['rtl_cycles']} "
        f"| {r['gvsoc_cycles']} | {r['cycles_error_pct']:+.2f}% | {r['rtl_dma_ns']:.0f} "
        f"| {r['gvsoc_dma_ns']:.0f} | {r['dma_error_pct']:+.2f}% | "
        f"{'PASS' if r['within_10pct'] else 'FAIL'} |" for r in results)
    (validation / 'VALIDATION.md').write_text(f'''# Additional workload validation

All cases use the original gather C kernel and runtime, compiled with the RTL
CMake toolchain flags. Each RTL/GVSoC pair executes the same ELF, checked against
the SHA-256 in [manifest.json](manifest.json). Only the generated data header
changes. Sorted cases use seed 42; the shuffled case uses seed 7.

The configuration is fixed across cases. No per-case latency correction is
applied. The 10% check applies separately to software cycles and the DMA window.

| Workload | Variant | RTL cycles | GVSoC cycles | Error | RTL DMA ns | GVSoC DMA ns | Error | Result |
|---|---|---:|---:|---:|---:|---:|---:|---|
{rows}

![Timing deviations](../plots/validation_deviation.png)

Machine-readable results: [CSV](results.csv), [JSON](results.json).
Build commands: [build-commands.json](build-commands.json).
Each case directory contains its data header, ELF files, build logs, and both
simulators' logs, exit codes and DRAMSys transaction databases.
''')
    return results


if __name__ == '__main__':
    output = Path(__file__).resolve().parents[2] / 'build/sparse_dma'
    rows = analyze(output)
    for row in rows:
        print(f"{row['case']}/{row['variant']}: cycles {row['cycles_error_pct']:+.2f}%, "
              f"DMA {row['dma_error_pct']:+.2f}%")
    raise SystemExit(0 if all(r['within_10pct'] for r in rows) else 1)
