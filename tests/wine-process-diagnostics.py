#!/usr/bin/env python3
"""Check the actual process-counter formatter against the selected Apple SDK."""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
work = Path(sys.argv[1]).resolve()
work.mkdir(parents=True, exist_ok=True)
sdk = sys.argv[2] if len(sys.argv) > 2 else 'macosx'
source = (root / 'build/ntdll-unix/server_ios.c').read_text()
start = source.index('            n = snprintf( line, sizeof(line),\n                          "[xp]')
end = source.index('            if (qf &&', start)
probe = work / 'wine-process-diagnostics.c'
probe.write_text('''#include <stdio.h>
#include <stdint.h>
#include <sys/resource.h>
#include <mach/mach_time.h>
#define XP_MS(ticks) ((double)(ticks) * tb.numer / tb.denom / 1e6)
int check_process_diagnostics(void) {
    struct rusage_info_v6 ru = {0}, pru = {0};
    mach_timebase_info_data_t tb = {1, 1};
    uint64_t now = 0, t_start = 0;
    double dt_ms = 0, cpu = 0, sum_thr_ms = 0, pms = 0, ems = 0;
    double pcy = 0, cy = 0, ins = 0;
    char wall[16] = "00:00:00", line[1400];
    int n;
''' + source[start:end] + '''    return n;
}
''')
command = ['xcrun', '--sdk', sdk, 'clang', '-std=c11', '-fsyntax-only',
           '-Wall', '-Wextra', '-Werror', str(probe)]
if sdk == 'iphoneos':
    sdk_path = subprocess.check_output(['xcrun', '--sdk', sdk, '--show-sdk-path'], text=True).strip()
    command.extend(['-target', 'arm64-apple-ios17.0', '-isysroot', sdk_path])
subprocess.run(command, check=True)
print(f'Wine process diagnostic SDK check passed ({sdk})')
