#!/usr/bin/env python3
"""Snapshot changed sources, patches and input hashes alongside measurements."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


def record(repo, out):
    def git(directory, *args):
        return subprocess.check_output(['git', '-C', str(directory), *args], text=True).strip()
    sources = set()
    snapshot = out / 'source-snapshot'
    snapshot.mkdir(exist_ok=True)
    commits = {}
    for name, directory in (('gvsoc', repo), ('pulp', repo/'pulp'), ('core', repo/'core'),
                             ('rtl', repo.parent/'spatz-sparse-dma')):
        commits[name] = git(directory, 'rev-parse', 'HEAD')
        if name in ('pulp', 'core'):
            (snapshot / f'{name}.patch').write_text(git(directory, 'diff', '--binary') + '\n')
            for path in git(directory, 'diff', '--name-only').splitlines():
                sources.add(directory/path)
    sources.update((repo/'scripts/sparse_dma').glob('*'))
    sources.update(repo/'pulp'/path for path in (
        'targets/spatz_sparse_dma.py', 'targets/sparse_dma_test.py', 'tests/sparse_dma/model.cpp',
        'pulp/snitch/sparse_dma_uart.cpp', 'pulp/snitch/sparse_dma_dram_bridge.cpp',
        'pulp/snitch/sparse_dma_icache_prefetch.cpp'))
    checksums = {}
    for source in sorted(sources):
        if not source.is_file():
            continue
        relative = source.relative_to(repo)
        dest = snapshot / relative
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, dest)
        checksums[str(relative)] = hashlib.sha256(source.read_bytes()).hexdigest()
    dram_source = Path((out/'rtl-dramsys-source.txt').read_text().strip())
    commits['rtl_dramsys'] = git(dram_source, 'rev-parse', 'HEAD')
    document = dict(base_commits=commits, source_sha256=checksums,
        toolchain=dict(gvsoc='GCC 14.2.0', rtl_simulator='Questa 2025.3',
                       riscv='Spatz LLVM 14.0.6 (2023.08.10)', systemc_gvsoc='3.0.1'),
        dram_adapter_sha256=hashlib.sha256((out/'dramsys-api-adapter.patch').read_bytes()).hexdigest(),
        model_test=(out/'logs/model-test.log').read_text().strip())
    (out/'provenance.json').write_text(json.dumps(document, indent=2) + '\n')
    return document
