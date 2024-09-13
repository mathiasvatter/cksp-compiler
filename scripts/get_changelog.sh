#!/bin/bash

BUILD_DIR="cmake-build-release"
SUBMODULE_DIR="cksp-compiler-issues"
CHANGELOG_DIR="${SUBMODULE_DIR}/changelogs"
SUBMODULE_BRANCH="cksp-releases"

# Check if cksp executable exists
if [ ! -f "$BUILD_DIR/cksp" ]; then
    echo "Error: cksp executable not found in $BUILD_DIR." >&2
    exit 1
fi

# Check if the submodule is on the correct branch
check_submodule_branch() {
    pushd "$SUBMODULE_DIR" >/dev/null || exit 1
    CURRENT_SUBMODULE_BRANCH=$(git rev-parse --abbrev-ref HEAD)

    if [[ "$CURRENT_SUBMODULE_BRANCH" != "$SUBMODULE_BRANCH" ]]; then
        echo "Error: Submodule '$SUBMODULE_DIR' is not on the '$SUBMODULE_BRANCH' branch. Please switch to the correct branch." >&2
        popd >/dev/null
        exit 1
    fi
    popd >/dev/null
}

# Execute cksp --version and extract the version number
VERSION=$("$BUILD_DIR/cksp" --version | awk '{print $3}')
CURRENT_TAG="v${VERSION}"

# Funktion, um das Vorhandensein eines Tags zu überprüfen
check_tag_exists() {
    if ! git rev-parse "$1" >/dev/null 2>&1; then
        echo "Error: Tag '$1' wurde nicht gefunden." >&2
        exit 1
    fi
}

# Überprüfen, ob TAG1 älter ist als TAG2
check_tags_order() {
    if git merge-base --is-ancestor "$1" "$2"; then
        # Tag1 ist ein Vorfahre von Tag2 (d.h. Tag1 ist älter oder gleich Tag2)
        return 0
    else
        # Tag2 ist älter als Tag1
        return 1
    fi
}

# Check if the submodule is on the correct branch
check_submodule_branch

# Ensure the changelog directory exists
if [ ! -d "$CHANGELOG_DIR" ]; then
    mkdir -p "$CHANGELOG_DIR"
fi

# Falls zwei Tags als Argumente übergeben werden
if [[ $# -eq 2 ]]; then
    TAG1="$1"
    TAG2="$2"
    
    # Überprüfe, ob die beiden Tags existieren
    check_tag_exists "$TAG1"
    check_tag_exists "$TAG2"
    
    # Überprüfen, ob TAG1 älter ist als TAG2
    if ! check_tags_order "$TAG1" "$TAG2"; then
        echo "Error: $TAG2 is older than $TAG1. Please provide tags in the correct order (older first)." >&2
        exit 1
    fi

    # Changelog-Dateiname basierend auf dem neueren Tag (TAG2)
    CHANGELOG_FILE="${CHANGELOG_DIR}/CHANGELOG_${TAG2}.md"

    # Hole die Commit-Nachrichten zwischen den beiden Tags und speichere sie in der Changelog-Datei
    git log "${TAG1}..${TAG2}" --pretty=format:"%s" > "$CHANGELOG_FILE"
    echo "Changelog von ${TAG1} bis ${TAG2} wurde in $CHANGELOG_FILE geschrieben."

# Falls keine Tags als Argumente übergeben werden
elif [[ $# -eq 0 ]]; then
    # Finde den letzten Tag (gehe davon aus, dass Tags Versionsnummern entsprechen)
    LAST_TAG=$(git describe --tags --abbrev=0 --match "v*" 2>/dev/null) # Unterdrückt Fehlermeldungen

    # Überprüfe, ob LAST_TAG leer ist (d.h., es gibt keine vorherigen Tags)
    if [[ -z "$LAST_TAG" ]]; then
        echo "Dies ist der erste Release. Kein vorheriger Tag zum Vergleichen."
        CHANGELOG_FILE="${CHANGELOG_DIR}/CHANGELOG_${CURRENT_TAG}.md"
        git log --pretty=format:"%s" > "$CHANGELOG_FILE"
        echo "Changelog von HEAD wurde in $CHANGELOG_FILE geschrieben."
    else
        # Changelog-Dateiname basierend auf dem aktuellen Tag (CURRENT_TAG)
        CHANGELOG_FILE="${CHANGELOG_DIR}/CHANGELOG_${CURRENT_TAG}.md"

        # Hole die Commit-Nachrichten zwischen dem letzten Tag und HEAD
        git log "${LAST_TAG}..HEAD" --pretty=format:"%s" > "$CHANGELOG_FILE"
        echo "Changelog von ${LAST_TAG} bis HEAD wurde in $CHANGELOG_FILE geschrieben."
    fi

# Wenn die Anzahl der Argumente falsch ist
else
    echo "Usage: $0 [<tag1> <tag2>]" >&2
    exit 1
fi

pushd "$SUBMODULE_DIR" >/dev/null || exit 1
PUSH_SCRIPT="push_changelog.sh"
# Führt das push_changelog.sh Skript aus, wenn das Changelog existiert
if [ -f "$PUSH_SCRIPT" ]; then
    echo "Running ${PUSH_SCRIPT}..."
    bash "$PUSH_SCRIPT"
else
    echo "Error: ${PUSH_SCRIPT} does not exist. Aborting."
    exit 1
fi
# Zurück ins ursprüngliche Verzeichnis
popd >/dev/null