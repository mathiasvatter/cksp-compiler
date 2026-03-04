#!/bin/bash

BUILD_DIR="cmake-build-release"

# Extract version from the binary
VERSION=$("$BUILD_DIR/cksp" --version | awk '{print $3}')

# Check whether this is a pre-release version
if [[ "$VERSION" == *"-"* ]]; then
    VERSION_DIR="cksp_v${VERSION}"
else
    VERSION_DIR="cksp_v${VERSION}_release"
fi

# Print result
echo "$VERSION_DIR"
