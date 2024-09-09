#!/bin/bash

# Save current branch
CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)

# Überprüfe auf uncommited Changes
if git diff-index --quiet HEAD --; then
    # Keine Änderungen, wechsel zu Master-Branch
    git checkout main
else
    # Es gibt uncommited Änderungen
    echo "Error: There are uncommitted changes. Please commit or stash them before running this script." >&2
    exit 1
fi

BUILD_DIR="cmake-build-release"

# Check if cksp executable exists
if [ ! -f "$BUILD_DIR/cksp" ]; then
    echo "Error: cksp executable not found in $BUILD_DIR." >&2
    exit 1
fi

# Execute cksp --version and extract the version number
VERSION=$("$BUILD_DIR/cksp" --version | awk '{print $3}')

# Create a name for the release folder
RELEASES_DIR="_Releases"
if [ ! -d "$RELEASES_DIR" ]; then
    mkdir -p "$RELEASES_DIR"
fi

VERSION_DIR="cksp_v${VERSION}_release"
RELEASE_DIR="${RELEASES_DIR}/${VERSION_DIR}"

# Variables
REPO="mathiasvatter/ksp-compiler"
TAG="v${VERSION}"
RELEASE_NAME=$TAG
# Reading the changelog content
CHANGELOG_PATH="CHANGELOG.md"
if [ -f "$CHANGELOG_PATH" ]; then
    BODY=$(<"$CHANGELOG_PATH")
else
    echo "Error: CHANGELOG.md file not found." >&2
    exit 1
fi

# Ensure the assets path is correct
ASSETS_PATH="${RELEASE_DIR}.zip"
if [ ! -f "$ASSETS_PATH" ]; then
    echo "Error: .zip package of latest release not found in $RELEASE_DIR." >&2
    exit 1
fi

# Create the release
gh release create "$TAG" \
  --repo "$REPO" \
  --title "$RELEASE_NAME" \
  --notes "$BODY" \
  "$ASSETS_PATH"
#  --draft \

# Switch back to the original branch
git checkout "$CURRENT_BRANCH"