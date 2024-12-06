#!/bin/bash

BUILD_DIR="cmake-build-release"

# Version aus der Binärdatei extrahieren
VERSION=$("$BUILD_DIR/cksp" --version | awk '{print $3}')

# Überprüfen, ob es sich um eine Pre-Release-Version handelt
if [[ "$VERSION" == *"-"* ]]; then
    VERSION_DIR="cksp_v${VERSION}"
else
    VERSION_DIR="cksp_v${VERSION}_release"
fi

# Ergebnis ausgeben
echo "$VERSION_DIR"
