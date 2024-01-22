#!/bin/bash

BUILD_DIR="cmake-build-release"

cmake -DCMAKE_BUILD_TYPE=Release "-DCMAKE_MAKE_PROGRAM=C:/Program Files/JetBrains/CLion 2023.3.2/bin/ninja/win/x64/ninja.exe" -G Ninja -S . -B $BUILD_DIR

# Build the project using Ninja
cmake --build $BUILD_DIR -- -j 8

# Check if cksp executable exists
if [ ! -f "$BUILD_DIR/cksp.exe" ]; then
    echo "Error: cksp executable not found in $BUILD_DIR."
    exit 1
fi

# Run cksp --version and extract the version number
VERSION=$("$BUILD_DIR/cksp.exe" --version | awk '{print $3}')

# Create a name for the release folder
RELEASES_DIR="_Releases"

if [ ! -d "$RELEASES_DIR" ]; then
    mkdir -p "$RELEASES_DIR"
fi

RELEASE_DIR="${RELEASES_DIR}/cksp_v${VERSION}_release"
ARM_DIR="windows"
INTEL_DIR="windows"
WINDOWS_DIR="windows"

# Create the release folder
mkdir -p "$RELEASE_DIR"

# Create a readme file
"$BUILD_DIR/cksp.exe" --help >> "$RELEASE_DIR/readme.txt"

mkdir -p "$RELEASE_DIR/$ARM_DIR"
mkdir -p "$RELEASE_DIR/$INTEL_DIR"
mkdir -p "$RELEASE_DIR/$WINDOWS_DIR"

# Copy the binary files to the release folder
cp "$BUILD_DIR/cksp.exe" "$RELEASE_DIR/$WINDOWS_DIR/cksp.exe"

# Zip it up
zip -vr "${RELEASE_DIR}.zip" $RELEASE_DIR -x "*.DS_Store"

echo "Release files copied to $RELEASE_DIR"
