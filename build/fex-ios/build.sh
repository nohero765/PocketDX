#!/bin/bash
# Configure and build the FEXCore static libraries the app links
# (FEX/build-ios/FEXCore/Source/*.a and External/*). Cross-build for the
# device with generic CPU tuning and offline telemetry disabled.
set -eu
R="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
B="$R/FEX/build-ios"
# iOS CMake does not infer SYSTEM_PROCESSOR from OSX_ARCHITECTURES.
# Reconfigure on retries so an incomplete cache cannot retain the empty value.
cmake -S "$R/FEX" -B "$B" -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
    -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_MACOSX_BUNDLE=OFF \
    -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0 -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-include \"$R/build/fex-ios/native-diagnostics.h\"" \
    -DTUNE_CPU=none -DTUNE_ARCH=generic -DENABLE_OFFLINE_TELEMETRY=OFF \
    -DBUILD_TESTING=OFF -DBUILD_THUNKS=OFF -DBUILD_FEXCONFIG=OFF -DBUILD_FEX_LINUX_TESTS=OFF \
    -DENABLE_FEX_ALLOCATOR=OFF -DENABLE_ASSERTIONS=OFF -DENABLE_CLANG_THUNKS=ON -DENABLE_CCACHE=ON
cmake --build "$B" --target FEXCore FEXCore_Base --parallel "${MADEIRA_BUILD_JOBS:-2}"
ls "$B/FEXCore/Source/"*.a
