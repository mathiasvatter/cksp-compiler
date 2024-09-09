#!/bin/bash

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
BODY="$("$BUILD_DIR/cksp" --help)"

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
  --draft \
  "$ASSETS_PATH"
