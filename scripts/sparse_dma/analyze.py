#!/usr/bin/env python3
"""Extract measured cycles, check the 10% target, and create plots/report."""
import csv
import json
import re
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
from analyze_validation import analyze as analyze_validation
from provenance import record as record_provenance

repo = Path(__file__).resolve().parents[2]
out = repo / 'build/sparse_dma'
rtl = json.loads((out / 'rtl-results.json').read_text())['results']
config = json.loads((repo / 'scripts/sparse_dma/config.json').read_text())
results = []
for reference in rtl:
    variant = reference['variant']
    run = out / 'runs' / variant
    if json.loads((run / 'config.json').read_text()) != config:
        raise RuntimeError(f'{variant}: recorded configuration differs from the current calibration')
    log = re.sub(r'\x1b\[[0-9;]*m', '', (run / 'simulation.log').read_text())
    measured = re.search(r'The gather took (\d+) cycles', log)
    if measured is None:
        raise RuntimeError(f'{variant}: no software cycle measurement in {run}/simulation.log')
    begins = [tuple(map(int, match)) for match in re.findall(r'DMA_BEGIN id=(\d+) cycle=(\d+)', log)]
    ends = {int(i): int(t) for i, t in re.findall(r'DMA_END id=(\d+) cycle=(\d+)', log)}
    if len(begins) < 2:
        raise RuntimeError(f'{variant}: missing DMA traces; run with --trace=idma --trace-level=trace')
    gather = begins[1:]  # First descriptor is the untimed index preload.
    expected_descriptors = 1 if variant == 'gather-hw' else 128
    if len(gather) != expected_descriptors or any(i not in ends for i, _ in gather):
        raise RuntimeError(f'{variant}: missing or extra DMA descriptors/completions')
    start = gather[0][1]
    end = max(ends[i] for i, _ in gather)
    cycles = int(measured.group(1))
    window = end - start  # Both reference and target use a 1 GHz cluster clock.
    cycles_error = 100 * (cycles / reference['cycles'] - 1)
    window_error = 100 * (window / reference['dma_window_ns'] - 1)
    status = int((run / 'exit-status.txt').read_text())
    correct = (reference['correct'] and reference['exit_status'] == 0
        and status == 0 and 'CORRECT!' in log
        and bool(re.search(r'\[EOC\] retval=0 ', log))
        and not re.search(r'WRONG!|Mismatch gather|fatal|Invalid access', log))
    results.append(dict(variant=variant, correct=correct,
        rtl_cycles=reference['cycles'], gvsoc_cycles=cycles, cycles_error_pct=cycles_error,
        rtl_dma_ns=reference['dma_window_ns'], gvsoc_dma_ns=window, dma_error_pct=window_error,
        rtl_GB_s=32768/reference['dma_window_ns'], gvsoc_GB_s=32768/window,
        descriptors=len(gather), index_reads=len(re.findall(r'INDEX_READ', log)),
        dma_begin_cycle=start, dma_end_cycle=end,
        within_10pct=correct and abs(cycles_error) <= 10 and abs(window_error) <= 10,
        log=f'runs/{variant}/simulation.log'))

(out / 'results.json').write_text(json.dumps(dict(results=results,
    all_within_10pct=all(r['within_10pct'] for r in results)), indent=2) + '\n')
with (out / 'results.csv').open('w') as stream:
    writer = csv.DictWriter(stream, fieldnames=results[0].keys())
    writer.writeheader()
    writer.writerows(results)

validation = []
manifest = out / 'validation/manifest.json'
if manifest.exists():
    cases = json.loads(manifest.read_text())
    if all((out/'validation'/c['case']/sim/c['variant']/'exit-status.txt').exists()
           for c in cases for sim in ('rtl', 'gvsoc')):
        for case in cases:
            recorded = out/'validation'/case['case']/'gvsoc'/case['variant']/'config.json'
            if json.loads(recorded.read_text()) != config:
                raise RuntimeError(f'Inconsistent calibration configuration: {recorded}')
        validation = analyze_validation(out)
provenance = record_provenance(repo, out)
all_results = results + validation
summary = dict(
    comparisons=len(all_results),
    correct_pairs=sum(r.get('correct', r.get('rtl_correct') and r.get('gvsoc_correct'))
                      for r in all_results),
    comparisons_within_10pct=sum(r['within_10pct'] for r in all_results),
    all_within_10pct=all(r['within_10pct'] for r in all_results),
    maximum_software_error_pct=max(abs(r['cycles_error_pct']) for r in all_results),
    maximum_dma_error_pct=max(abs(r['dma_error_pct']) for r in all_results),
    threshold_pct=10,
    original_results='results.json', additional_results='validation/results.json')
(out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
compatibility_log = out / 'logs/pulp-open-ddr-regression.log'
compatibility_status = out / 'logs/pulp-open-ddr-exit-status.txt'
compatibility_passed = (compatibility_log.exists() and compatibility_status.exists()
    and 'TEST SUCCESS' in compatibility_log.read_text()
    and compatibility_status.read_text().strip() == '0')

plots = out / 'plots'
plots.mkdir(exist_ok=True)
plt.rcParams.update({'font.size': 11, 'axes.spines.top': False, 'axes.spines.right': False,
                     'figure.dpi': 140, 'savefig.bbox': 'tight'})
labels = [r['variant'] for r in results]
x = np.arange(len(results))
colors = ['#315b96', '#de8232']

def save(fig, name):
    for extension in ('png', 'svg', 'pdf'):
        fig.savefig(plots / f'{name}.{extension}')
    plt.close(fig)

fig, axes = plt.subplots(1, 2, figsize=(11, 4))
for ax, suffix, ylabel in zip(axes, ('cycles', 'dma_ns'), ('Software-measured cycles', 'DMA window (ns)')):
    for offset, prefix, color in zip((-0.18, 0.18), ('rtl', 'gvsoc'), colors):
        values = [r[f'{prefix}_{suffix}'] for r in results]
        bars = ax.bar(x + offset, values, .34, label=prefix.upper(), color=color)
        ax.bar_label(bars, fmt='%.0f', padding=3)
    ax.set_xticks(x, labels)
    ax.set_ylabel(ylabel)
    ax.set_ylim(0, ax.get_ylim()[1]*1.13)
    ax.grid(axis='y', alpha=.18)
axes[0].legend(frameon=False)
fig.suptitle('Sparse-DMA gather: RTL and GVSoC (identical ELF binaries, 32 KiB)')
fig.tight_layout()
save(fig, 'cycles')

fig, ax = plt.subplots(figsize=(8, 4))
ax.axhspan(-10, 10, color='#6da567', alpha=.16, label='±10% target')
for offset, key, label, color in zip((-.18,.18), ('cycles_error_pct','dma_error_pct'),
                                    ('Software cycles','DMA window'), colors):
    bars = ax.bar(x+offset, [r[key] for r in results], .34, label=label, color=color)
    ax.bar_label(bars, fmt='%+.1f%%', padding=3)
ax.axhline(0, color='black', linewidth=.6)
ax.set_xticks(x, labels)
ax.set_ylabel('(GVSoC − RTL) / RTL (%)')
ax.set_title('Timing deviation, checked per variant and measurement window')
limit = max(14, max(abs(r[k]) for r in results for k in ('cycles_error_pct', 'dma_error_pct')) * 1.4)
ax.set_ylim(-limit, limit)
ax.legend(frameon=False, loc='lower center', ncol=3, fontsize=9)
save(fig, 'deviation')

fig, ax = plt.subplots(figsize=(8, 4))
for offset, prefix, color in zip((-.18,.18), ('rtl','gvsoc'), colors):
    bars = ax.bar(x+offset, [r[f'{prefix}_GB_s'] for r in results], .34, label=prefix.upper(), color=color)
    ax.bar_label(bars, fmt='%.2f', padding=3)
ax.axhline(57.55, linestyle='--', color='#777777', label='RTL guide peak: 57.55 GB/s')
ax.set_xticks(x, labels)
ax.set_ylabel('32768 bytes / DMA window (GB/s)')
ax.set_title('Gather transfer bandwidth')
ax.set_ylim(0, 65)
ax.legend(frameon=False)
save(fig, 'bandwidth')

fig, ax = plt.subplots(figsize=(9, 4))
for offset, prefix, color in zip((-.18,.18), ('rtl','gvsoc'), colors):
    dma = [r[f'{prefix}_dma_ns'] for r in results]
    overhead = [r[f'{prefix}_cycles'] - r[f'{prefix}_dma_ns'] for r in results]
    ax.bar(x+offset, dma, .34, color=color, label=f'{prefix.upper()} DMA window')
    ax.bar(x+offset, overhead, .34, bottom=dma, color=color, alpha=.35,
           label=f'{prefix.upper()} remaining timed code')
ax.set_xticks(x, labels)
ax.set_ylabel('Cycles at 1 GHz')
ax.set_title('Measured DMA interval and remaining software timing interval')
ax.legend(frameon=False, fontsize=9)
save(fig, 'timing_breakdown')

rows = '\n'.join(
    f"| {r['variant']} | {r['rtl_cycles']} | {r['gvsoc_cycles']} | {r['cycles_error_pct']:+.2f}% "
    f"| {r['rtl_dma_ns']:.0f} | {r['gvsoc_dma_ns']} | {r['dma_error_pct']:+.2f}% "
    f"| {'PASS' if r['within_10pct'] else 'FAIL'} |" for r in results)
history = []
for iteration in sorted((out/'iterations').glob('*')):
    data = json.loads((iteration/'results.json').read_text())['results']
    history.append(f"| [{iteration.name}](iterations/{iteration.name}/REPORT.md) | "
        f"{max(abs(r['cycles_error_pct']) for r in data):.2f}% | "
        f"{max(abs(r['dma_error_pct']) for r in data):.2f}% |")
validation_text = 'Additional workloads have not all completed.'
if validation:
    passed = sum(r['within_10pct'] for r in validation)
    validation_text = f'''**{passed}/{len(validation)} additional cases meet both 10% checks.**
{sum(r['rtl_correct'] and r['gvsoc_correct'] for r in validation)}/{len(validation)} pairs pass the data and termination checks.
These use G=64, G=256, and G=128 with shuffled indices, each in all three variants.
See the [complete validation table](validation/VALIDATION.md) and
[raw CSV](validation/results.csv).

![Additional workload deviations](plots/validation_deviation.png)

The additional cases exposed missing DRAM beat pacing and instruction-cache
prefetching during development. They therefore form part of the final
calibration/regression set, rather than an independent accuracy guarantee.
'''
report = f'''# Sparse-DMA gather calibration

Acceptance requires correct data and at most 10% absolute relative deviation
for each variant's software cycle count and DMA issue-to-completion interval.
Original R2048/D128/G128 experiment: **{'PASS' if all(r['within_10pct'] for r in results) else 'NOT YET WITHIN TARGET'}**.
Across all {summary['comparisons']} comparisons, {summary['comparisons_within_10pct']} meet both timing checks
and the data/termination checks. Maximum absolute software-cycle deviation:
**{summary['maximum_software_error_pct']:.2f}%**; maximum DMA-window deviation:
**{summary['maximum_dma_error_pct']:.2f}%**. See [summary.json](summary.json).

| Variant | RTL cycles | GVSoC cycles | Error | RTL DMA ns | GVSoC DMA ns | Error | Result |
|---|---:|---:|---:|---:|---:|---:|---|
{rows}

The first DMA descriptor preloads the indices and is excluded from the gather
window, following the RTL measurement. At the shared 1 GHz cluster frequency,
one nanosecond is one cycle. Bandwidth uses 32768 payload bytes divided by the
DMA window; software setup and polling remain included in the separate cycle
measurement. The two intervals are not interchangeable.

## Reproduction

From the GVSoC repository, run:

```bash
bash scripts/sparse_dma/step.sh prepare
bash scripts/sparse_dma/step.sh build-dram
bash scripts/sparse_dma/step.sh build
bash scripts/sparse_dma/step.sh test
bash scripts/sparse_dma/step.sh run gather
bash scripts/sparse_dma/step.sh run gather-opt
bash scripts/sparse_dma/step.sh run gather-hw
source build/reproduce_dramsys/env.sh
python3 scripts/sparse_dma/analyze.py
# Additional workload comparison:
bash scripts/sparse_dma/step.sh build-validation
bash scripts/sparse_dma/step.sh validate rtl
bash scripts/sparse_dma/step.sh validate gvsoc
bash scripts/sparse_dma/step.sh analyze
```

The original RTL ELF binaries and DRAM configuration files are copied without
changes. Their checksums are in [input-sha256.json](input-sha256.json). The
target uses the requested `pulp/pulp/idma` implementation with an io-v1 cluster
fabric, two Snitch/Spatz cores, 128 KiB TCDM in 16 banks, a dedicated 64-bit index
port, 512-bit DMA datapath, and the HBM2E-3600 memory configuration.
The board disables HTIF polling and requires the same successful EOC-register
write as the RTL testbench, together with the benchmark's full data check.

The L0 instruction caches each hold eight fully associative 32-byte lines,
with round-robin replacement and next-line/backward-branch/JAL prefetching.
The shared L1 instruction cache is 4 KiB, two ways, with 32-byte lines.
The DRAM bridge issues one 64-byte beat per cycle and bounds the outstanding
beat window at 64. Narrow reads fetch a full beat, as in the RTL AXI-Lite
adapter. DRAMSys controls DRAM bandwidth; TCDM writes remain limited to
64 bytes per cluster cycle. Fabric delay is consumed before timed DRAM
submission, avoiding a second bandwidth delay after the response.

The DRAMSys sources come from the already reproduced RTL dependency. They are
built against this checkout's SystemC 3.0.1 and GCC 14.2.0. The
[API adapter](dramsys-api-adapter.patch) changes the C `add_dram` entry point to
return GVSoC's memory metadata and accept DRAM-relative addresses. The DRAM
controller, scheduler, timing constraints, and address mapping are unchanged.

Each run saves its effective parameter file, GVSoC configuration, simulation
log, exit status, and DRAMSys transaction database under `runs/<variant>/`.
The current target parameters are:

```json
{(out / 'runs/gather/config.json').read_text().strip()}
```

## Plots and data

![Cycle comparison](plots/cycles.png)

![Timing deviation](plots/deviation.png)

![Bandwidth](plots/bandwidth.png)

![Timing intervals](plots/timing_breakdown.png)

All plots are also saved as SVG and PDF. Raw measured values are available as
[CSV](results.csv) and [JSON](results.json).

The focused model test covers all four index widths, a partial final index
word, descriptor snapshots, ordinary 1D/2D transfers, empty transfers, delayed
AXI/index responses, and small queues that force frontend stalls. Its output is
in [model-test.log](logs/model-test.log).

## Additional workloads

{validation_text}

## Calibration changes and audit trail

The original programs and reported cycle values are unmodified. Corrections
were made to the modeled system: direct index routing, fully associative L0,
single-stage core jump timing, continuous row forwarding, DRAM beat pacing,
instruction prefetching and round-robin replacement. One configuration applies
to all variants and sizes. The DRAM memory/controller timing values are the
copied RTL values throughout.

| Preserved iteration | Maximum software-cycle error | Maximum DMA-window error |
|---|---:|---:|
{chr(10).join(history)}
| Current | {max(abs(r['cycles_error_pct']) for r in results):.2f}% | {max(abs(r['dma_error_pct']) for r in results):.2f}% |

The iteration table refers to the three original G=128 workloads.
Each archived iteration includes the three original run logs, effective
configuration and measurements. Selected iterations also preserve additional
workload logs. The [initial latency sweep](parameter_sweep/results.json) tried
4, 6, 8, 10 and 12 cycles before the final packet-prefetch correction. None
met every additional-case check. After correcting prefetching and disabling
HTIF polling, the [final shared-delay sweep](parameter_sweep_eoc/results.json)
tested 8, 6 and 4 cycles against all 12 cases. Their worst timing deviations
were 12.66%, 12.77% and 8.94%, respectively. The final fabric delay is 4 cycles
for every case, with no per-workload correction.

The legacy router charges bandwidth to HTIF debug reads, so disabling polling
also changed execution timing. The DRAMSys transaction records show a refresh
overlapping the original hardware-gather DMA window with EOC-only termination
at an 8-cycle fabric delay. The final 4-cycle setting avoids this overlap in
that workload. This sensitivity to initial controller state is included in
the accuracy scope below; see [refresh-phase-evidence.json](refresh-phase-evidence.json).
[provenance.json](provenance.json) records base commits, source
hashes and the functional-test result. The [source snapshot](source-snapshot/)
contains the changed files and patches for both submodules.

The original `pulp-open-ddr` example was rebuilt and rerun after the shared
model changes: **{'TEST SUCCESS, exit status 0' if compatibility_passed else 'NOT VERIFIED'}**. Its compatibility-check output
is in [pulp-open-ddr-regression.log](logs/pulp-open-ddr-regression.log).

## Scope and remaining approximations

This is a transaction-level, cycle-calibrated model of the measured gather
workloads. It does not establish 10% accuracy for every application or model
every RTL handshake. The legacy ISS/cache and TCDM interconnect approximate
pipeline scheduling and arbitration. Instruction prefetching predicts from a
fetched 32-byte packet and covers branch fall-through because this ISS does not
send each instruction PC to the external cache. Refills remain serialized.
The DRAM adapter returns a whole DMA burst after its
last beat; it preserves beat-level controller traffic but does not stream
partial read data into the DMA. Reset during an active transfer, invalid
gather programming and performance under concurrent vector/CPU traffic have
not been calibrated. Functional gather requires aligned indices and a
power-of-two source stride, matching the supported RTL use case.

Measured accuracy applies to the software `mcycle` and DMA windows shown
above. Full boot-to-EOC simulation time includes setup and verification and
is not calibrated. Different boot timing can shift a short gather across a
DRAM refresh interval; accuracy outside the measured controller states needs
separate validation. Plot bandwidth is payload divided by the DMA interval,
not the DRAMSys whole-run average.
'''
(out / 'REPORT.md').write_text(report)
for r in results:
    print(f"{r['variant']}: cycles={r['gvsoc_cycles']} ({r['cycles_error_pct']:+.2f}%), "
          f"DMA={r['gvsoc_dma_ns']} ns ({r['dma_error_pct']:+.2f}%), correct={r['correct']}")
if validation:
    print(f"Additional cases within both 10% limits: {sum(r['within_10pct'] for r in validation)}/{len(validation)}")
raise SystemExit(0 if all(r['within_10pct'] for r in results + validation) else 1)
