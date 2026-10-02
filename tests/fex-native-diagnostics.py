#!/usr/bin/env python3
"""Compile and exercise the pinned fork's native-only diagnostic on the host."""
import os
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
work = Path(sys.argv[1]).resolve()
work.mkdir(parents=True, exist_ok=True)
source = (root / 'FEX/FEXCore/Source/Utils/ArchHelpers/Arm64.cpp').read_text()
start = source.index('static void IosLogUnimplementedCASPAL(')
end = source.index('\nstatic bool HandleCASPAL(', start)
probe = work / 'fex-native-diagnostics.cpp'
probe.write_text('''#include <cassert>
#include <cstdint>
#include <cstring>
#include <tuple>
#include "native-diagnostics.h"
namespace LogMan::Msg {
static int reports;
template<typename... Args> void EFmt(const char*, Args&&... args) {
  ++reports;
  // The unsupported region query must explicitly report unknown metadata.
  auto values = std::forward_as_tuple(args...);
  assert(std::get<5>(values) == nullptr);
  assert(std::get<6>(values) == 0);
  assert(std::get<7>(values) == 0);
  assert(std::strcmp(std::get<8>(values), "?") == 0);
  assert(std::get<9>(values) == 0);
}
}
namespace FEXCore::ArchHelpers::Arm64 {
''' + source[start:end] + '''
}
int main() {
  using namespace FEXCore::ArchHelpers::Arm64;
  uint64_t regs[32] {};
  regs[3] = 0x1000;
  IosLogUnimplementedCASPAL(1, regs, 3);
  assert(LogMan::Msg::reports == 0);
  regs[3] = 0x1001;
  IosLogUnimplementedCASPAL(0, regs, 3);
  assert(LogMan::Msg::reports == 0);
  for (int i = 0; i < 12; ++i) IosLogUnimplementedCASPAL(1, regs, 3);
  assert(LogMan::Msg::reports == 8);
  static_assert(FEXCore::Context::IosFfsBypassLog[0] == 0);
  static_assert(FEXCore::Context::IosCbEntryLog[6] == 0);
}
''')
# Tuple is only a test logger dependency, not part of the compatibility header.
compiler = os.environ.get('CXX', 'clang++')
subprocess.run([compiler, '-std=c++20', '-Wall', '-Wextra', '-Werror',
                '-D__APPLE__=1',
                '-I', str(root / 'build/fex-ios'), str(probe),
                '-o', str(work / 'fex-native-diagnostics')], check=True)
subprocess.run([str(work / 'fex-native-diagnostics')], check=True)
# The adapter must disappear entirely for Windows guest / non-Apple builds.
for flags in (['-D__APPLE__=1', '-DFEX_IOS_HOST=1'],
              ['-D__APPLE__=1', '-D_WIN32=1'], ['-U__APPLE__']):
    result = subprocess.run([compiler, '-std=c++20', '-E', '-P', '-x', 'c++',
                             *flags, str(root / 'build/fex-ios/native-diagnostics.h')],
                            check=True, capture_output=True, text=True)
    assert 'VirtualQuery' not in result.stdout
    assert 'IosFfsBypassLog' not in result.stdout
print('Native FEX diagnostic checks passed')
