#!/bin/bash

BUILD_DIR="cmake-build-release"

# Check if cksp executable exists
if [ ! -f "$BUILD_DIR/cksp" ]; then
    echo "Error: cksp executable not found in $BUILD_DIR." >&2
    exit 1
fi

# Finde den letzten Tag (gehe davon aus, dass Tags Versionsnummern entsprechen)
LAST_TAG=$(git describe --tags --abbrev=0 --match "v*" 2>/dev/null) # Unterdrückt Fehlermeldungen

# Überprüfe, ob LAST_TAG leer ist (d.h., es gibt keine vorherigen Tags)
if [[ -z "$LAST_TAG" ]]; then
    echo "Dies ist der erste Release. Kein vorheriger Tag zum Vergleichen."
    git log --pretty=format:"%s" > CHANGELOG.txt
else
    # Hole die Commit-Nachrichten zwischen dem letzten Tag und HEAD und speichere sie in CHANGELOG.txt
    git log "${LAST_TAG}..HEAD" --pretty=format:"%s" > CHANGELOG.txt
    echo "Changelog von ${LAST_TAG} bis HEAD wurde in CHANGELOG.txt geschrieben."
fi
