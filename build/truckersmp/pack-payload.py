#!/usr/bin/env python3
"""Package the supplied payload into GitHub-compatible chunks under 100 MB."""
import hashlib
import io
import json
from pathlib import Path
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[2]
TEMP = ROOT.parent / 'TMP/payload-index'
SOURCE = ROOT.parent / 'truckersmp-cli'
OUT = ROOT / 'test-payload'
subprocess.run(['python3', str(Path(__file__).with_name('stage-payload.py')),
                '--manifest-only', '--output', str(TEMP)], check=True)
manifest = (TEMP / 'payload.json').read_bytes()
entries = json.loads(manifest)['files']
OUT.mkdir(parents=True, exist_ok=True)

class Chunks:
    limit = 80 * 1024 * 1024
    def __init__(self):
        self.file = None
        self.records = []
        self.used = 0
    def write(self, data):
        size = len(data)
        data = memoryview(data)
        while data:
            if self.file is None:
                name = f'payload-{len(self.records):03}.part'
                self.file = (OUT / name).open('wb')
                self.records.append({'file': name})
                self.used = 0
            count = min(len(data), self.limit - self.used)
            self.file.write(data[:count])
            self.used += count
            data = data[count:]
            if self.used == self.limit: self.finish()
        return size
    def finish(self):
        if self.file:
            path = Path(self.file.name)
            self.file.close()
            with path.open('rb') as stream:
                checksum = hashlib.file_digest(stream, 'sha256').hexdigest()
            self.records[-1].update(bytes=path.stat().st_size, sha256=checksum)
            self.file = None

writer = Chunks()
def clean(info):
    info.uid = info.gid = 0
    info.uname = info.gname = ''
    info.mtime = 0
    info.mode = 0o644
    return info
with tarfile.open(fileobj=writer, mode='w|gz', compresslevel=1) as archive:
    info = clean(tarfile.TarInfo('payload.json'))
    info.size = len(manifest)
    archive.addfile(info, io.BytesIO(manifest))
    for item in entries:
        relative = item['path']
        path = SOURCE / 'TruckersMP' / relative
        if relative == 'runtime/d3dcompiler_47.dll': path = SOURCE / 'dlls/d3dcompiler_47.dll'
        if path.is_symlink(): raise SystemExit('Payload must contain regular files only')
        archive.add(path, arcname=relative, recursive=False, filter=clean)
writer.finish()
index = {'version': 1, 'parts': writer.records, 'payloadSHA256': hashlib.sha256(manifest).hexdigest()}
(OUT / 'index.json').write_text(json.dumps(index, indent=2) + '\n')
expected = {x['file'] for x in writer.records}
for part in OUT.glob('payload-*.part'):
    if part.name not in expected: part.unlink()
print(f'Payload packed: {len(expected)} chunks; {sum(x["bytes"] for x in writer.records):,} bytes')
