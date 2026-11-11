#!/bin/bash
set -euo pipefail
cmake -S /workspace/libdvfelplay -B /workspace/fel-build -G Ninja \
 -DCMAKE_TOOLCHAIN_FILE=/workspace/sdk/ndk/28.2.13676358/build/cmake/android.toolchain.cmake \
 -DANDROID_ABI=armeabi-v7a -DANDROID_PLATFORM=android-26 -DANDROID_STL=c++_static \
 -DCMAKE_BUILD_TYPE=RelWithDebInfo -DDVFP_BUILD_TEST=OFF -DDVFEL_KO= -DDVFEL_KO_SC2= -DLIBDOVI_DIR=/workspace/libdovi3-arm
cmake --build /workspace/fel-build --target dvfelplay -j2
mkdir -p /workspace/fel-prefix/lib /workspace/fel-prefix/include
cp /workspace/fel-build/libdvfelplay.so /workspace/fel-prefix/lib/
cp /workspace/libdvfelplay/include/dvfelplay.h /workspace/fel-prefix/include/
