// SPDX-License-Identifier: MIT
#pragma once

// The pinned fork reads these Windows-guest diagnostic counters even when
// FEX_IOS_HOST is disabled. The native frontend has neither the ARM64EC
// fast-forward thunk nor its callback-entry instrumentation.
// Keep their reports inactive; defining FEX_IOS_HOST here would also enable
// dependencies on Windows-only Mono bridge helpers.
#if defined(__cplusplus) && defined(__APPLE__) && !defined(FEX_IOS_HOST)
#include <cstdint>
namespace FEXCore::Context {
inline constexpr std::uint64_t IosFfsBypassLog[4] {};
inline constexpr std::uint64_t IosCbEntryLog[8] {};
}
#endif
