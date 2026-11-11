#!/bin/sh
# usage: ANDROID_NDK=/path/to/ndk ./build.sh        -> build/arm64-v8a/libresmod.so
set -e
: "${ANDROID_NDK:?set ANDROID_NDK to your NDK folder}"
cmake -S . -B build/arm64-v8a -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
      -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 -DCMAKE_BUILD_TYPE=Release
cmake --build build/arm64-v8a
echo "built: build/arm64-v8a/libresmod.so"
