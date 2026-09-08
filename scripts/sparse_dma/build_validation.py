#!/usr/bin/env python3
"""Build additional shapes from the original RTL kernel and CMake toolchain flags."""
import hashlib
import importlib.util
import json
from pathlib import Path
import shlex
import subprocess

import numpy as np

repo = Path(__file__).resolve().parents[2]
rtl = repo.parent / 'spatz-sparse-dma'
kernel = rtl / 'sw/spatzBenchmarks/gather'
build = rtl / 'hw/system/spatz_cluster/sw/build/spatzBenchmarks'
out = repo / 'build/sparse_dma/validation'
out.mkdir(parents=True, exist_ok=True)
spec = importlib.util.spec_from_file_location('rtl_gather_generator', kernel / 'script/gen_data.py')
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)
manifest = []
commands = []
for count, seed, pattern in ((64, 42, 'sorted'), (256, 42, 'sorted'), (128, 7, 'shuffled')):
    name = f'R2048_D128_G{count}_S{seed}_{pattern}'
    case_dir = out / name
    case_dir.mkdir(exist_ok=True)
    rng = np.random.default_rng(seed)
    matrix = rng.standard_normal((2048, 128)).astype(np.float16)
    indices = rng.choice(2048, size=count, replace=False).astype(np.uint32)
    if pattern == 'sorted':
        indices.sort()
    header = case_dir / 'data.h'
    header.write_text('// Generated using the RTL gather data generator.\n'
        '#include "layer.h"\n'
        f'const gather_layer gather_l = {{.ROWS=2048, .DIM=128, .G={count}, .dtype=FP16}};\n'
        + generator.c_fp16_array('gather_matrix_dram', matrix) + '\n'
        + generator.c_u32_array('gather_index_dram', indices) + '\n')
    for variant in ('gather', 'gather-opt', 'gather-hw'):
        template = build / f'CMakeFiles/test-spatzBenchmarks-{variant}_R2048_D128_G128.dir'
        flags = dict(line.split(' = ', 1) for line in (template / 'flags.make').read_text().splitlines()
                     if line.startswith('C_') and ' = ' in line)
        link = shlex.split((template / 'link.txt').read_text())
        obj = case_dir / f'{variant}.o'
        binary = case_dir / f'{variant}.elf'
        defines = [flag for flag in shlex.split(flags['C_DEFINES']) if not flag.startswith('-DDATAHEADER=')]
        compile_cmd = [link[0], *defines, f'-DDATAHEADER="{header}"',
            *shlex.split(flags['C_INCLUDES']), '-I' + str(kernel / 'data'),
            *shlex.split(flags['C_FLAGS']), '-c', str(kernel / 'main.c'), '-o', str(obj)]
        for i, token in enumerate(link):
            if token.endswith('/gather/main.c.o'):
                link[i] = str(obj)
        link[link.index('-o') + 1] = str(binary)
        log_path = case_dir / f'{variant}-build.log'
        with log_path.open('w') as log:
            for command in (compile_cmd, link):
                commands.append(dict(cwd=str(build), argv=command))
                log.write(shlex.join(command) + '\n')
                log.flush()
                subprocess.run(command, cwd=build, stdout=log, stderr=subprocess.STDOUT, check=True)
        manifest.append(dict(case=name, variant=variant, rows=2048, dim=128, count=count,
            seed=seed, pattern=pattern, bytes_transferred=count*256,
            binary=str(binary.relative_to(out)), sha256=hashlib.sha256(binary.read_bytes()).hexdigest()))
        print(f'Built {name}/{variant}', flush=True)
(out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
(out / 'build-commands.json').write_text(json.dumps(commands, indent=2) + '\n')
