#!/bin/bash

BUILD_DIR="cmake-build-release"
SUBMODULE_DIR="cksp-compiler-issues"
CHANGELOG_DIR="${SUBMODULE_DIR}/changelogs"
SUBMODULE_BRANCH="cksp-releases"


VERSION=$("$BUILD_DIR/cksp" --version | awk '{print $3}')
CURRENT_TAG="v${VERSION}"

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

# Check if the submodule is on the correct branch
check_submodule_branch

# push changelog.md to submodule
cp CHANGELOG.md "$CHANGELOG_DIR/CHANGELOG_${CURRENT_TAG}.md"

pushd "$SUBMODULE_DIR" >/dev/null || exit 1

# Commit and push the changelog
git add "changelogs/CHANGELOG_${CURRENT_TAG}.md"
git commit -m "Add changelog for ${CURRENT_TAG}"
git push origin "$SUBMODULE_BRANCH"

# back to main repo
popd >/dev/null