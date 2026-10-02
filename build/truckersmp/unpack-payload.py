#!/usr/bin/env python3
"""Verify every chunk and extracted file before it enters the app bundle."""
import hashlib
import json
from pathlib import Path
import shutil
import tarfile

ROOT = Path(__file__).resolve().parents[2]
parts = ROOT / 'test-payload'
temporary = (ROOT.parent if (ROOT.parent / 'truckersmp-cli').is_dir() else ROOT) / 'TMP'
temporary.mkdir(exist_ok=True)
index = json.loads((parts / 'index.json').read_text())
archive = temporary / 'truckersmp-payload.tar.gz'
with archive.open('wb') as output:
    for item in index['parts']:
        name = item['file']
        if Path(name).name != name: raise SystemExit('Invalid payload chunk name')
        path = parts / name
        with path.open('rb') as stream:
            checksum = hashlib.file_digest(stream, 'sha256').hexdigest()
        if checksum != item['sha256'] or path.stat().st_size != item['bytes']:
            raise SystemExit('Payload chunk checksum mismatch: ' + name)
        with path.open('rb') as stream: shutil.copyfileobj(stream, output)
stage = temporary / 'verified-payload'
if stage.exists(): shutil.rmtree(stage)
stage.mkdir()
with tarfile.open(archive, 'r:gz') as source:
    source.extractall(stage, filter='data')
manifest = (stage / 'payload.json').read_bytes()
if hashlib.sha256(manifest).hexdigest() != index['payloadSHA256']:
    raise SystemExit('Payload manifest checksum mismatch')
expected = {'payload.json'}
for item in json.loads(manifest)['files']:
    path = stage / item['path']
    if path.resolve().is_relative_to(stage.resolve()) is False or not path.is_file():
        raise SystemExit('Invalid payload file')
    with path.open('rb') as stream:
        checksum = hashlib.file_digest(stream, 'sha256').hexdigest()
    if checksum != item['sha256'] or path.stat().st_size != item['bytes']:
        raise SystemExit('Payload file checksum mismatch: ' + item['path'])
    expected.add(item['path'])
actual = {p.relative_to(stage).as_posix() for p in stage.rglob('*') if p.is_file()}
if actual != expected: raise SystemExit('Unexpected payload files')
out = ROOT / 'app/Madeira/TruckersMP'
if out.exists(): shutil.rmtree(out)
shutil.move(stage, out)
archive.unlink()
print('ETS2 payload verified and staged')
