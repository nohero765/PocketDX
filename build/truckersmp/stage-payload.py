#!/usr/bin/env python3
"""Stage only the supplied ETS2 and shared payload; exclude ATS and helpers."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, default=ROOT.parent / 'truckersmp-cli')
parser.add_argument('--output', type=Path, default=ROOT / 'app/Madeira/TruckersMP')
parser.add_argument('--manifest-only', action='store_true')
args = parser.parse_args()
source = args.source / 'TruckersMP'
core = source / 'core_ets2mp.dll'
if not core.is_file():
    raise SystemExit('Supplied core_ets2mp.dll is missing')
files = [core]
for directory in ('data/ets2', 'data/shared', 'licenses'):
    folder = source / directory
    if not folder.is_dir():
        raise SystemExit(f'Supplied {directory} folder is missing')
    files.extend(sorted(p for p in folder.rglob('*') if p.is_file() and p.name != '.DS_Store'))
manifest = []
for path in files:
    relative = path.relative_to(source).as_posix()
    with path.open('rb') as stream:
        checksum = hashlib.file_digest(stream, 'sha256').hexdigest()
    manifest.append({'path': relative, 'bytes': path.stat().st_size, 'sha256': checksum})
    if not args.manifest_only:
        target = args.output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        if not target.is_file() or target.stat().st_size != path.stat().st_size:
            shutil.copy2(path, target)
        else:
            with target.open('rb') as stream:
                if hashlib.file_digest(stream, 'sha256').hexdigest() != checksum:
                    shutil.copy2(path, target)
runtime = args.source / 'dlls/d3dcompiler_47.dll'
if not runtime.is_file():
    raise SystemExit('Supplied d3dcompiler_47.dll is missing')
with runtime.open('rb') as stream:
    checksum = hashlib.file_digest(stream, 'sha256').hexdigest()
manifest.append({'path': 'runtime/d3dcompiler_47.dll', 'bytes': runtime.stat().st_size, 'sha256': checksum})
if not args.manifest_only:
    target = args.output / 'runtime/d3dcompiler_47.dll'
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(runtime, target)
args.output.mkdir(parents=True, exist_ok=True)
(args.output / 'payload.json').write_text(json.dumps({'version': 1, 'files': manifest}, indent=2) + '\n')
print(f'ETS2 payload: {len(manifest)} files, {sum(x["bytes"] for x in manifest):,} bytes')
