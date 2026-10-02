// SPDX-License-Identifier: MIT
#pragma once

// The pinned fork reads these Windows-guest diagnostic counters even when
// FEX_IOS_HOST is disabled. The native frontend has neither the ARM64EC
// fast-forward thunk nor its callback-entry instrumentation.
// Keep their reports inactive; defining FEX_IOS_HOST here would also enable
// dependencies on Windows-only Mono bridge helpers.
#if defined(__cplusplus) && defined(__APPLE__) && !defined(_WIN32) && !defined(FEX_IOS_HOST)
#include <cstddef>
#include <cstdint>
namespace FEXCore::Context {
inline constexpr std::uint64_t IosFfsBypassLog[4] {};
inline constexpr std::uint64_t IosCbEntryLog[8] {};
}

// The fork also queries Windows region metadata solely for its unsupported
// CASPAL diagnostic. Native Apple code cannot query Wine's guest mapping table.
// Report query failure (type="?", zero metadata), preserving the fault report
// and the existing atomic emulation. Scope this adapter to that diagnostic's
// namespace; it must never replace the Windows allocator's real VirtualQuery.
namespace FEXCore::ArchHelpers::Arm64 {
using LPCVOID = const void*;
struct MEMORY_BASIC_INFORMATION {
  void* BaseAddress {};
  std::size_t RegionSize {};
  std::uint32_t Protect {}, Type {}, State {};
};
inline constexpr std::uint32_t MEM_IMAGE = 0x1000000;
inline constexpr std::uint32_t MEM_MAPPED = 0x40000;
inline std::size_t VirtualQuery(LPCVOID, MEMORY_BASIC_INFORMATION* info, std::size_t) {
  *info = {};
  return 0;
}
}
#endif
