#!/usr/bin/env python3
"""Keep an immutable calibration iteration before changing the model/config."""
import argparse
from pathlib import Path
import shutil

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('name')
parser.add_argument('notes')
args = parser.parse_args()
if '/' in args.name or args.name.startswith('.'):
    parser.error('name must be a simple directory name')
out = Path(__file__).resolve().parents[2] / 'build/sparse_dma'
archive = out / 'iterations' / args.name
archive.mkdir(parents=True, exist_ok=False)
for name in ('results.json', 'results.csv', 'REPORT.md', 'plots', 'runs'):
    source = out / name
    if source.is_dir():
        shutil.copytree(source, archive / name)
    else:
        shutil.copy2(source, archive / name)
shutil.copy2(out / 'runs/gather/config.json', archive / 'config.json')
(archive / 'notes.txt').write_text(args.notes + '\n')
print(archive)
