#!/bin/sh
# Builds the bench/ tools into build/bench/.
#
# The tools are ordinary CMake targets now, so this is a convenience wrapper
# rather than a second build system -- it exists so `bench/build.sh` keeps
# working, and so the tools build the same way on every platform. On Windows use
# the CMake commands directly:
#
#   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
#   cmake --build build --config Release --target tools
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SAM=$(dirname "$HERE")
BUILD=${BUILD_DIR:-$SAM/build}

cmake -S "$SAM" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" --target tools -j

echo "tools are in $BUILD/bench"
