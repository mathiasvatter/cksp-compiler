#!/usr/bin/env bash
set -euo pipefail

# Publishes the generated CHANGELOG.md to the cksp-compiler-issues submodule,
# by copying it to the submodule's changelogs directory, committing and pushing it.
# Expects the following environment variables to be set:
# - SUBMODULE_TOKEN: A GitHub token with permissions to push to the submodule repo
# - TAG: The tag name for the current release (e.g., "v1.2.3")

# ------------------------------------------------------------
# Required environment variables
# ------------------------------------------------------------
: "${SUBMODULE_TOKEN:?SUBMODULE_TOKEN not set}"
: "${TAG:?TAG not set}"

# ------------------------------------------------------------
# Config
# ------------------------------------------------------------
SUBMODULE_DIR="cksp-compiler-issues"
SUBMODULE_BRANCH="cksp-releases"
CHANGELOG_SUBDIR="changelogs"
SOURCE_CHANGELOG="CHANGELOG.md"

# ------------------------------------------------------------
# Init submodule
# ------------------------------------------------------------
git submodule update --init --recursive "${SUBMODULE_DIR}"

pushd "${SUBMODULE_DIR}" >/dev/null

# ------------------------------------------------------------
# Rewrite remote to HTTPS + PAT
# ------------------------------------------------------------
url="$(git config --get remote.origin.url)"

repo="$url"
repo="${repo%.git}"
repo="${repo%/}"
repo="${repo#*github.com:}"
repo="${repo#*github.com/}"

if [[ -z "$repo" || "$repo" == "$url" ]]; then
  echo "ERROR: could not extract owner/repo from: $url" >&2
  exit 1
fi

git remote set-url origin "https://x-access-token:${SUBMODULE_TOKEN}@github.com/${repo}.git"

echo "Submodule remote:"
git remote -v | sed -E 's#x-access-token:[^@]+@#x-access-token:***@#'

# ------------------------------------------------------------
# Checkout / create release branch
# ------------------------------------------------------------
git fetch origin "${SUBMODULE_BRANCH}" || true
if git show-ref --verify --quiet "refs/remotes/origin/${SUBMODULE_BRANCH}"; then
  git checkout -B "${SUBMODULE_BRANCH}" "origin/${SUBMODULE_BRANCH}"
else
  git checkout -B "${SUBMODULE_BRANCH}"
  mkdir -p "${CHANGELOG_SUBDIR}"
  git add "${CHANGELOG_SUBDIR}"
  git -c user.name="GitHub Actions" -c user.email="actions@github.com" \
    commit -m "init ${CHANGELOG_SUBDIR}" || true
  git push -u origin "${SUBMODULE_BRANCH}"
fi

# ------------------------------------------------------------
# Copy changelog
# ------------------------------------------------------------
mkdir -p "${CHANGELOG_SUBDIR}"
TARGET_FILE="${CHANGELOG_SUBDIR}/CHANGELOG_${TAG}.md"
cp -f "../${SOURCE_CHANGELOG}" "${TARGET_FILE}"

# ------------------------------------------------------------
# Commit & push
# ------------------------------------------------------------
git config user.name "GitHub Actions"
git config user.email "actions@github.com"

git add "${TARGET_FILE}"
if git diff --cached --quiet; then
  echo "No changes in submodule."
else
  git commit -m "Add changelog for ${TAG}"
  git push origin "HEAD:${SUBMODULE_BRANCH}"
fi

popd >/dev/null

# ------------------------------------------------------------
# Update superproject pointer
# ------------------------------------------------------------
git add "${SUBMODULE_DIR}"
if git diff --cached --quiet; then
  echo "No superproject pointer change."
else
  git commit -m "Publish Release: ${TAG}"
fi