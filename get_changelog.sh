#!/bin/bash

BUILD_DIR="cmake-build-release"

# Check if cksp executable exists
if [ ! -f "$BUILD_DIR/cksp" ]; then
    echo "Error: cksp executable not found in $BUILD_DIR." >&2
    exit 1
fi

# Execute cksp --version and extract the version number
CURRENT_VERSION=$("$BUILD_DIR/cksp" --version | awk '{print $3}')

# Finde den letzten Tag (gehe davon aus, dass Tags Versionsnummern entsprechen)
LAST_TAG=$(git describe --tags --abbrev=0 --match "v*" 2>/dev/null) # Unterdrückt Fehlermeldungen

# Definiere den aktuellen Tag basierend auf der aktuellen Version
CURRENT_TAG="v${CURRENT_VERSION}"

# Überprüfe, ob LAST_TAG leer ist (d.h., es gibt keine vorherigen Tags)
if [[ -z "$LAST_TAG" ]]; then
    echo "Dies ist der erste Tag: ${CURRENT_TAG}. Kein vorheriger Tag zum Vergleichen."
    git log --pretty=format:"%s" > CHANGELOG.txt
else
    # Hole die Commit-Nachrichten zwischen den beiden Tags und speichere sie in CHANGELOG.txt
    if [[ "$LAST_TAG" != "$CURRENT_TAG" ]]; then
        git log "${LAST_TAG}..${CURRENT_TAG}" --pretty=format:"%s" > CHANGELOG.txt
        echo "Changelog von ${LAST_TAG} bis ${CURRENT_TAG} wurde in CHANGELOG.txt geschrieben."
    else
        echo "Keine neuen Commits seit dem letzten Tag (${LAST_TAG}). Keine Aktualisierung des Changelogs erforderlich."
    fi
fi
