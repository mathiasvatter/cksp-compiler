#!/bin/bash

ARM_DIR="macos_arm64"
INTEL_DIR="macos_x86_64"
WINDOWS_DIR="windows"

# Prüfe, ob das Skript in einer CI-Umgebung läuft
if [ "$CI" == "true" ]; then
    # Verwende das global installierte cmake im CI-Runner
    CMAKE_DIR=$(which cmake)
else
    # Lokale Umgebung: Prüfe, ob ein benutzerdefinierter CMake-Pfad gesetzt ist
    if [ -z "$CUSTOM_CMAKE_DIR" ]; then
        # Fallback auf Standardpfade
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

# Überprüfen, auf welcher macOS-Architektur man sich befindet und setze target_dir
ARCH=$(uname -m)
if [ "$ARCH" == "arm64" ]; then
    TARGET_DIR="$ARM_DIR"
else
    TARGET_DIR="$INTEL_DIR"
fi

# Überprüfe, ob cmake gefunden wurde
if [ -z "$CMAKE_DIR" ] || [ ! -x "$CMAKE_DIR" ]; then
    echo "Error: cmake not found or not executable." >&2
    exit 1
fi

BUILD_DIR="cmake-build-release"

# Build and deploy executable
$CMAKE_DIR -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -S .
if [ $? -ne 0 ]; then
    echo "Error: CMake configuration failed." >&2
    exit 1
fi

$CMAKE_DIR --build "$BUILD_DIR" -- -j 8
if [ $? -ne 0 ]; then
    echo "Error: Build failed." >&2
    exit 1
fi

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

TAG="v${VERSION}"
# Überprüfen, ob es sich um eine Pre-Release-Version handelt (z.B. -alpha, -beta, -rc)
if [[ "$VERSION" == *"-"* ]]; then
    VERSION_DIR="cksp_v${VERSION}"
else
    VERSION_DIR="cksp_v${VERSION}_release"
fi
RELEASE_DIR="${RELEASES_DIR}/${VERSION_DIR}"

# Erstellen des Release-Ordners
mkdir -p "$RELEASE_DIR"

# Kopieren von README.md und CHANGELOG.md in den Release-Ordner
cp README.md "$RELEASE_DIR/README.md"
cp CHANGELOG.md "$RELEASE_DIR/CHANGELOG.md"

mkdir -p "$RELEASE_DIR/$ARM_DIR"
mkdir -p "$RELEASE_DIR/$INTEL_DIR"
mkdir -p "$RELEASE_DIR/$WINDOWS_DIR"

rm "$RELEASE_DIR/$TARGET_DIR/cksp"
# Kopieren der Binärdateien in den Release-Ordner
cp "$BUILD_DIR/cksp" "$RELEASE_DIR/$TARGET_DIR/cksp"

# Erstellen eines Namens für den Release-Ordner
WIKI_DIR="ksp-compiler.wiki"
# check if cksp exists
if [ -d "$WIKI_DIR" ]; then
  cd "$WIKI_DIR"
  ./export_wiki.sh

  mv "The CKSP Dialect.pdf" "$RELEASE_DIR/The CKSP Dialect.pdf"
  cd -
fi

# Wechseln in das Verzeichnis _Releases vor dem Zippen
cd "$RELEASES_DIR"
# zipping it up
zip -vr "${VERSION_DIR}.zip" "$VERSION_DIR" -x "*.DS_Store"
# Zurück zum ursprünglichen Verzeichnis
cd -

echo "Release files copied to $RELEASE_DIR"
