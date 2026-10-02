#!/usr/bin/env python3
"""Build the unpatched server members that Madeira's script otherwise needs prebuilt."""
from pathlib import Path
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build/wineserver/obj'
OUT.mkdir(parents=True, exist_ok=True)
script = (ROOT / 'build/wineserver/build.sh').read_text()
section = script.split('PATCHED_FILES=(', 1)[1].split('\n)', 1)[0]
replaced = set(re.findall(r'"[^"\n]+:([^:"\n]+\.o)"', section))
source = (ROOT / 'wine/server/Makefile.in').read_text()
names = re.findall(r'^\s+([\w]+\.c)\s', source, flags=re.MULTILINE)
sdk = subprocess.check_output(['xcrun', '--sdk', 'iphoneos', '--show-sdk-path'], text=True).strip()
flags = ['-arch', 'arm64', '-isysroot', sdk, '-miphoneos-version-min=17.0', '-O2',
         '-DWINE_IOS=1', '-D__WINESRC__', '-Dmain=wineserver_main', '-DHAVE_LINUX_NTSYNC_H=1',
         '-DBINDIR="/usr/local/bin"', '-DDATADIR="/usr/local/share"',
         '-Wno-implicit-function-declaration']
for folder in ['wine/include', 'wine/include/wine', 'wine/build-macos/include',
               'wine/server', 'build/wineserver', 'build/ntdll-unix/shims', 'build/madsync']:
    flags += ['-I', str(ROOT / folder)]
for header in ['build/wineserver/config_ios.h', 'build/wineserver/unicode_fix.h',
               'build/wineserver/wineserver_ios_kill.h']:
    flags += ['-include', str(ROOT / header)]
flags += ['-include', 'stdarg.h']
objects = []
for name in names:
    object_name = Path(name).with_suffix('.o').name
    if object_name in replaced: continue
    out = OUT / ('base-' + object_name)
    subprocess.run(['xcrun', '--sdk', 'iphoneos', 'clang', *flags,
                    '-c', str(ROOT / 'wine/server' / name), '-o', str(out)], check=True)
    objects.append(out)
if not objects: raise SystemExit('No Wine server sources found')
library = OUT / 'libwineserver.a'
library.unlink(missing_ok=True)
subprocess.run(['xcrun', 'ar', 'rcs', str(library), *map(str, objects)], check=True)
print(f'Base server: {len(objects)} source files')
