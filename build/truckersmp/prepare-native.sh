#!/bin/bash
# Build absent native archives from the pinned Madeira sources on a Mac runner.
set -euo pipefail
R="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$R"
T="$R/TMP"
if [ -d "$R/../truckersmp-cli" ]; then T="$R/../TMP"; fi
mkdir -p "$T" toolchains
export TMPDIR="$T/"
export CLANG_MODULE_CACHE_PATH="$T/clang-modules"
export CCACHE_DIR="$T/ccache"
JOBS="${MADEIRA_BUILD_JOBS:-2}"
TC="$R/toolchains/llvm-mingw-20260421-ucrt-macos-universal"
if [ ! -x "$TC/bin/arm64ec-w64-mingw32-clang" ]; then
    curl --fail --location --retry 3 \
        https://github.com/mstorsjo/llvm-mingw/releases/download/20260421/llvm-mingw-20260421-ucrt-macos-universal.tar.xz \
        -o "$T/llvm-mingw.tar.xz"
    echo "bd85a3975723815cef28dbbd2ca2cb0c926f6b348a12a0453f39f7af273cb3f7  $T/llvm-mingw.tar.xz" | shasum -a 256 --check
    tar -xJf "$T/llvm-mingw.tar.xz" -C toolchains
    rm "$T/llvm-mingw.tar.xz"
fi
export PATH="$TC/bin:$PATH"
SDK="$(xcrun --sdk iphoneos --show-sdk-path)"

# DXMT's AIR converter needs LLVM built for the device, plus a host tablegen.
LLVM_REV=8dfdcc7b7
if [ ! -d toolchains/llvm-project/llvm ]; then
    curl --fail --location --retry 3 "https://github.com/llvm/llvm-project/archive/$LLVM_REV.tar.gz" -o "$T/llvm.tar.gz"
    mkdir -p toolchains/llvm-project
    tar -xzf "$T/llvm.tar.gz" -C toolchains/llvm-project --strip-components=1
    rm "$T/llvm.tar.gz"
fi
if [ ! -x toolchains/llvm-host-build/bin/llvm-tblgen ]; then
    cmake -S toolchains/llvm-project/llvm -B toolchains/llvm-host-build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD= -DLLVM_ENABLE_PROJECTS= \
        -DLLVM_INCLUDE_TESTS=OFF -DLLVM_ENABLE_ZLIB=OFF
    cmake --build toolchains/llvm-host-build --target llvm-tblgen --parallel "$JOBS"
fi
cmake -S toolchains/llvm-project/llvm -B toolchains/llvm-ios-build -G Ninja \
    -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_SYSROOT="$SDK" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0 -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_HOST_TRIPLE=arm64-apple-ios17.0 -DLLVM_DEFAULT_TARGET_TRIPLE=arm64-apple-ios17.0 \
    -DLLVM_TARGET_ARCH=host -DLLVM_TARGETS_TO_BUILD= -DLLVM_ENABLE_PROJECTS= \
    -DLLVM_BUILD_TOOLS=OFF -DLLVM_INCLUDE_TESTS=OFF -DLLVM_INCLUDE_BENCHMARKS=OFF \
    -DLLVM_ENABLE_ZLIB=OFF -DLLVM_ENABLE_ZSTD=OFF -DLLVM_ENABLE_LIBXML2=OFF \
    -DLLVM_TABLEGEN="$R/toolchains/llvm-host-build/bin/llvm-tblgen"
cmake --build toolchains/llvm-ios-build --parallel "$JOBS"

bash build/gnutls-ios/build.sh
bash build/ffmpeg/build.sh
if [ ! -d research/freetype ]; then
    git clone --depth 1 --branch VER-2-13-3 https://github.com/freetype/freetype.git research/freetype
fi
bash build/freetype-ios/build.sh
bash build/fex-ios/build.sh
cmake --build FEX/build-ios --target JemallocLibs --parallel "$JOBS"

# The checked-in PE farm and xtajit64.dll remain pinned; rebuild the two
# modules that implement the startup hook, not unrelated Windows modules.
python3 build/truckersmp/patch-wine.py
mkdir -p wine/build-macos
if [ ! -f wine/build-macos/config.status ]; then
    (cd wine/build-macos && ../configure --enable-win64 --without-x --disable-tests)
fi
make -C wine/build-macos -j "$JOBS" tools/winebuild/winebuild tools/widl/widl
make -C wine/build-macos -j "$JOBS" include/all
bash build/wine-pe/build-ntdll.sh
make -C wine/build-arm64ec -j "$JOBS" dlls/kernelbase/arm64ec-windows/kernelbase.dll
"$TC/bin/arm64ec-w64-mingw32-strip" wine/build-arm64ec/dlls/kernelbase/arm64ec-windows/kernelbase.dll
cp wine/build-arm64ec/dlls/kernelbase/arm64ec-windows/kernelbase.dll app/Madeira/arm64ec-windows/

python3 build/truckersmp/build-server-base.py
bash build/wineserver/build.sh
bash build/ntdll-unix/build.sh
bash build/win32u-unix/build.sh
bash build/dxmt-ios/build.sh
archives=(toolchains/llvm-ios-build/lib/*.a)
xcrun libtool -static -o app/Madeira/libdxmt_combined.a build/dxmt-ios/libdxmt_unix.a "${archives[@]}"
LLVM_MINGW="$TC/bin" bash build/madeira-dock/build.sh --check
mkdir -p app/Madeira/x86_64-vcruntime
bash build/stage-licenses.sh
