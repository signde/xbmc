#!/usr/bin/env python3
"""Refresh the bundled PPI from --source PATH, or verify the recorded snapshot."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[2]
target = root / 'addons/script.signde.tinyppi'
record = root / 'tools/firetv-fel/ppi-source.json'
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path)
args = parser.parse_args()

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

if args.source:
    source = args.source.resolve()
    def git(*values):
        return subprocess.check_output(['git', '-C', str(source), *values]).decode()
    assert git('branch', '--show-current').strip() == 'Piers-Cube3-FEL', 'Wrong PPI source branch'
    names = git('ls-files', '--cached', '--others', '--exclude-standard', '-z').split('\0')
    files = {}
    for name in names:
        if not name or name.startswith(('.github/', 'tests/')) or name.endswith(('.pyc', '.zip')):
            continue
        path = source / name
        if not path.is_file():
            continue
        dest = target / name
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, dest)
        files[name] = sha(path)
    if record.exists():
        for name in json.loads(record.read_text())['files'].keys() - files.keys():
            (target / name).unlink(missing_ok=True)
    record.write_text(json.dumps({
        'repository': 'https://github.com/signde/script.tinyppi',
        'branch': 'Piers-Cube3-FEL',
        'base_commit': git('rev-parse', 'HEAD').strip(),
        'files': files,
    }, indent=2) + '\n')
manifest = json.loads(record.read_text())
for name, digest in manifest['files'].items():
    assert sha(target / name) == digest, f'PPI snapshot differs: {name}'
print(f"Verified {len(manifest['files'])} bundled PPI source files")
