#!/bin/bash

# Verzeichnisse und Dateien
SUBMODULE_DIR="cksp-compiler-issues"
CHANGELOG_DIR="${SUBMODULE_DIR}/changelogs"
BUILD_DIR="cmake-build-release"
PUBLIC_REPO="mathiasvatter/${SUBMODULE_DIR}"

# Hole die aktuelle Version des Tags (z.B. v1.0.0)
CURRENT_TAG=$("$BUILD_DIR/cksp" --version | awk '{print $3}')
TAG="v${CURRENT_TAG}"


# Create a name for the release folder
RELEASES_DIR="_Releases"
if [ ! -d "$RELEASES_DIR" ]; then
    mkdir -p "$RELEASES_DIR"
fi

VERSION_DIR="cksp_v${CURRENT_TAG}_release"
RELEASE_DIR="${RELEASES_DIR}/${VERSION_DIR}"
# Ensure the assets path is correct
ASSETS_PATH="${RELEASE_DIR}.zip"
if [ ! -f "$ASSETS_PATH" ]; then
    echo "Error: .zip package of latest release not found in $RELEASE_DIR." >&2
    exit 1
fi

# Überprüfe, ob das Changelog-File existiert
CHANGELOG_FILE="${CHANGELOG_DIR}/CHANGELOG_${TAG}.md"
if [ ! -f "$CHANGELOG_FILE" ]; then
    echo "Error: ${CHANGELOG_FILE} does not exist. Aborting."
    exit 1
fi
BODY=$(<"$CHANGELOG_PATH")


# Wechsle ins Submodule-Verzeichnis
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

# Lösche den Tag, wenn er bereits existiert
if git rev-parse "$TAG" >/dev/null 2>&1; then
    echo "Tag '$TAG' exists. Deleting the tag..."
    git tag -d "$TAG"
    git push --delete origin "$TAG"
fi

# Füge einen neuen Tag hinzu und pushe ihn
echo "Creating and pushing new tag '$TAG'..."
git tag "$TAG"
git push origin "$TAG"

# Release auf GitHub erstellen
echo "Creating a release for the current commit..."
# Create the new release
gh release create "$TAG" \
  --repo "$PUBLIC_REPO" \
  --title "$TAG" \
  --notes "$BODY" \
  "$ASSETS_PATH"

# Zurück ins ursprüngliche Verzeichnis
popd >/dev/null

echo "Release published and tagged."
