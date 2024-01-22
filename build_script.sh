#!/bin/bash

ARM_DIR="macos_arm64"
INTEL_DIR="macos_x86_64"
WINDOWS_DIR="windows"

# Überprüfen, auf welcher macOS-Architektur man sich befindet
ARCH=$(uname -m)
if [ "$ARCH" == "arm64" ]; then
    TARGET_DIR="$ARM_DIR"
    CMAKE_DIR="/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cmake"
else
    TARGET_DIR="$INTEL_DIR"
    CMAKE_DIR="/Applications/CLion.app/Contents/bin/cmake/mac/bin/cmake"
fi

BUILD_DIR="cmake-build-release"

# build and deploy executable
$CMAKE_DIR -B $BUILD_DIR -DCMAKE_BUILD_TYPE=Release -S . && $CMAKE_DIR --build $BUILD_DIR -- -j 8

# check if cksp exists
if [ ! -f "$BUILD_DIR/cksp" ]; then
    echo "Error: cksp executable not found in $BUILD_DIR."
    exit 1
fi

# Ausführen von cksp --version und Extrahieren der Versionsnummer
VERSION=$("$BUILD_DIR/cksp" --version | awk '{print $3}')

# Erstellen eines Namens für den Release-Ordner
RELEASES_DIR="_Releases"
# check if cksp exists
if [ ! -d "$RELEASES_DIR" ]; then
	mkdir -p "$RELEASES_DIR"
fi

RELEASE_DIR="${RELEASES_DIR}/cksp_v${VERSION}_release"

# Erstellen des Release-Ordners
mkdir -p "$RELEASE_DIR"

# create readme file
"$BUILD_DIR/cksp" --help >> "$RELEASE_DIR/readme.txt"

mkdir -p "$RELEASE_DIR/$ARM_DIR"
mkdir -p "$RELEASE_DIR/$INTEL_DIR"
mkdir -p "$RELEASE_DIR/$WINDOWS_DIR"

# Kopieren der Binärdateien in den Release-Ordner
cp "$BUILD_DIR/cksp" "$RELEASE_DIR/$TARGET_DIR/cksp"

# zipping it up
zip -vr "${RELEASE_DIR}.zip" "$RELEASE_DIR" -x "*.DS_Store"

echo "Release files copied to $RELEASE_DIR"
