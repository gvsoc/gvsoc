#!/usr/bin/env python3
"""Materialize unchanged RTL binaries/configuration in the local evidence tree."""
import hashlib
import json
from pathlib import Path
import shutil

repo = Path(__file__).resolve().parents[2]
rtl = repo.parent / 'spatz-sparse-dma'
out = repo / 'build/sparse_dma'
config = rtl / 'hw/system/spatz_cluster/tb/dram_config'
resources = out / 'dram/dramsys_configs'
manifest = {}
files = {
    'HBM2E-3600.json': 'HBM2E-3600.json',
    'ms_hbm2e_16Gb_3600.json': 'memspec/ms_hbm2e_16Gb_3600.json',
    'mc_hbm2e_fr_fcfs_grp.json': 'mcconfig/mc_hbm2e_fr_fcfs_grp.json',
    'am_hbm2e_16Gb_pc_brc.json': 'addressmapping/am_hbm2e_16Gb_pc_brc.json',
    'simconfig_hbm2e.json': 'simconfig/simconfig_hbm2e.json',
}
for name, relative in files.items():
    dest = resources / relative
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(config / name, dest)
    manifest[str(dest.relative_to(out))] = hashlib.sha256(dest.read_bytes()).hexdigest()
    # DRAMSys 5 resolves these names relative to the simulation JSON; the RTL
    # library also searches category subdirectories. Keep identical copies.
    if dest != resources / name:
        shutil.copy2(config / name, resources / name)
for variant in ('gather', 'gather-opt', 'gather-hw'):
    source = rtl / f'hw/system/spatz_cluster/sw/build/spatzBenchmarks/test-spatzBenchmarks-{variant}_R2048_D128_G128'
    dest = out / f'binaries/{variant}.elf'
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, dest)
    manifest[str(dest.relative_to(out))] = hashlib.sha256(dest.read_bytes()).hexdigest()
shutil.copy2(rtl / 'build/reproduce_sparse_dma/results.json', out / 'rtl-results.json')
(out / 'input-sha256.json').write_text(json.dumps(manifest, indent=2) + '\n')
print('Copied unchanged RTL ELF binaries and HBM2E-3600 configurations.')
