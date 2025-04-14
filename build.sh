#!/bin/bash

# Usage: ./build_only.sh [release|debug]
BUILD_TYPE="${1:-release}"  # Standard: release

# Validierung
if [[ "$BUILD_TYPE" != "release" && "$BUILD_TYPE" != "debug" ]]; then
    echo "Usage: $0 [release|debug]"
    exit 1
fi

# Setze Build-Verzeichnis und CMake-Typ
if [ "$BUILD_TYPE" == "debug" ]; then
    BUILD_DIR="cmake-build-debug"
    CMAKE_BUILD_TYPE="Debug"
else
    BUILD_DIR="cmake-build-release"
    CMAKE_BUILD_TYPE="Release"
fi

# Architektur bestimmen oder per ENV setzen (z. B. ARCH_OVERRIDE)
ARCHITECTURE=${ARCH_OVERRIDE:-$(uname -m)}

# Architekturabhängige CMake-Auswahl
if [ "$CI" == "true" ]; then
    # CI-Umgebung: cmake vom System verwenden
    CMAKE_DIR=$(which cmake)
else
    # Lokale Umgebung
    if [ -z "$CUSTOM_CMAKE_DIR" ]; then
        ARCH=$(uname -m)
        if [ "$ARCH" == "arm64" ]; then
            CMAKE_DIR="/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cmake"
        else
            CMAKE_DIR="/Applications/CLion.app/Contents/bin/cmake/mac/x64/bin/cmake"
        fi
    else
        CMAKE_DIR="$CUSTOM_CMAKE_DIR"
    fi
fi

# Prüfen ob CMake verfügbar ist
if [ -z "$CMAKE_DIR" ] || [ ! -x "$CMAKE_DIR" ]; then
    echo "Error: cmake not found or not executable." >&2
    exit 1
fi

echo "Building in $BUILD_TYPE mode..."
echo "Using CMake: $CMAKE_DIR"
echo "Build directory: $BUILD_DIR"

# Konfiguration
$CMAKE_DIR -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" -DCMAKE_OSX_ARCHITECTURES="$ARCHITECTURE" -S .
if [ $? -ne 0 ]; then
    echo "Error: CMake configuration failed." >&2
    exit 1
fi

# Kompilierung
$CMAKE_DIR --build "$BUILD_DIR" -- -j 8
if [ $? -ne 0 ]; then
    echo "Error: Build failed." >&2
    exit 1
fi

# Prüfen ob Binary existiert
if [ ! -f "$BUILD_DIR/cksp" ]; then
    echo "Error: cksp executable not found in $BUILD_DIR."
    exit 1
fi

chmod +x "$BUILD_DIR/cksp"

echo "✅ Build successful: $BUILD_DIR/cksp"
