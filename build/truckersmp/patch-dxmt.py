#!/usr/bin/env python3
"""Resolve parent-owned headers through explicit build include paths."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
folder = root / 'dxmt/src/winemetal/unix'
patches = (
    ('winemetal_unix.c', '../../../../../build/madeira_cfg.h', 'madeira_cfg.h', 'build/madeira_cfg.h'),
    ('winemetal_unix.c', '../../../../remote-metal/host/wmt_decode.h', 'remote-metal/host/wmt_decode.h', 'research/remote-metal/host/wmt_decode.h'),
    ('wmt_remote_client.h', '../../../../remote-metal/protocol.h', 'remote-metal/protocol.h', 'research/remote-metal/protocol.h'),
    ('wmt_remote_pack.h', '../../../../remote-metal/wmt_pack.h', 'remote-metal/wmt_pack.h', 'research/remote-metal/wmt_pack.h'),
)
for filename, old_path, new_path, dependency in patches:
    source = folder / filename
    old, new = f'#include "{old_path}"', f'#include "{new_path}"'
    text = source.read_text()
    if not (root / dependency).is_file():
        raise SystemExit(f'Missing DXMT dependency: {dependency}')
    if text.count(old) == 1 and new not in text:
        source.write_text(text.replace(old, new, 1))
    elif text.count(new) != 1 or old in text:
        raise SystemExit(f'Unexpected include in {filename}: inspect the pinned source before building')
print('DXMT parent header includes ready')
