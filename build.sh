#!/bin/bash

# Usage: ./build_only.sh [release|debug]
BUILD_TYPE="${1:-release}"  # Default: release

# Validation
if [[ "$BUILD_TYPE" != "release" && "$BUILD_TYPE" != "debug" ]]; then
    echo "Usage: $0 [release|debug]"
    exit 1
fi

# Set build directory and CMake type
if [ "$BUILD_TYPE" == "debug" ]; then
    BUILD_DIR="cmake-build-debug"
    CMAKE_BUILD_TYPE="Debug"
else
    BUILD_DIR="cmake-build-release"
    CMAKE_BUILD_TYPE="Release"
fi

# Determine architecture or override via ENV (e.g. ARCH_OVERRIDE)
ARCHITECTURE=${ARCH_OVERRIDE:-$(uname -m)}

# CI environment: use system cmake
CMAKE_DIR=$(which cmake)

# Check if CMake is available
if [ -z "$CMAKE_DIR" ] || [ ! -x "$CMAKE_DIR" ]; then
    echo "Error: cmake not found or not executable." >&2
    exit 1
fi

echo "Building in $BUILD_TYPE mode..."
echo "Using CMake: $CMAKE_DIR"
echo "Build directory: $BUILD_DIR"

# Configuration
$CMAKE_DIR -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
    -DCMAKE_OSX_ARCHITECTURES="$ARCHITECTURE" \
    -S .

if [ $? -ne 0 ]; then
    echo "Error: CMake configuration failed." >&2
    exit 1
fi

# Compilation
$CMAKE_DIR --build "$BUILD_DIR" -- -j 8
if [ $? -ne 0 ]; then
    echo "Error: Build failed." >&2
    exit 1
fi

# Check whether the binary exists
if [ ! -f "$BUILD_DIR/cksp" ]; then
    echo "Error: cksp executable not found in $BUILD_DIR."
    exit 1
fi

chmod +x "$BUILD_DIR/cksp"

echo "✅ Build successful: $BUILD_DIR/cksp"
