#!/usr/bin/env bash
set -euo pipefail

REPO="${GITHUB_REPOSITORY:-mathiasvatter/cksp-compiler}"
BUILD_DIR="cmake-build-release"
RELEASES_DIR="_Releases"

usage() {
  cat <<'EOF'
Usage: ./publish_latest_release.sh [--yes]

Builds CKSP on the current machine, updates the matching platform binary in
_Releases/<version>, zips the whole version directory including any existing
artifacts from other platforms, and publishes it to the GitHub releases of
mathiasvatter/cksp-compiler.

Options:
  --yes   Replace an existing release/tag without prompting.
EOF
}

ASSUME_YES=0
for arg in "$@"; do
  case "$arg" in
    --yes|-y)
      ASSUME_YES=1
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $arg" >&2
      usage >&2
      exit 1
      ;;
  esac
done

require_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Error: required command '$1' was not found." >&2
    exit 1
  fi
}

prepare_release_dir() {
  local release_dir="$1"

  mkdir -p "$release_dir/macos_arm64" "$release_dir/macos_x86_64" "$release_dir/windows"
  cp README.md "$release_dir/README.md"
}

confirm_replace() {
  if [ "${REPLACE_CONFIRMED:-0}" -eq 1 ]; then
    return 0
  fi

  if [ "$ASSUME_YES" -eq 1 ]; then
    REPLACE_CONFIRMED=1
    return 0
  fi

  printf "Release or tag %s already exists in %s. Replace it? [y/N] " "$TAG" "$REPO"
  read -r answer
  case "$answer" in
    y|Y|yes|YES)
      REPLACE_CONFIRMED=1
      ;;
    *)
      echo "Aborted."
      exit 1
      ;;
  esac
}

detect_platform() {
  case "$(uname -s)" in
    Darwin)
      echo "macos"
      ;;
    MINGW*|MSYS*|CYGWIN*)
      echo "windows"
      ;;
    *)
      echo "unsupported"
      ;;
  esac
}

create_zip() {
  local version_dir="$1"
  local zip_path="${RELEASES_DIR}/${version_dir}.zip"

  rm -f "$zip_path"

  if command -v zip >/dev/null 2>&1; then
    (cd "$RELEASES_DIR" && zip -qr "${version_dir}.zip" "$version_dir" -x "*.DS_Store")
  elif command -v powershell.exe >/dev/null 2>&1; then
    powershell.exe -NoProfile -Command \
      "Compress-Archive -Path '${RELEASES_DIR}/${version_dir}' -DestinationPath '${zip_path}' -Force"
  elif command -v 7z >/dev/null 2>&1; then
    7z a -tzip "$zip_path" "${RELEASES_DIR}/${version_dir}" -xr\!"*.DS_Store" >/dev/null
  else
    echo "Error: no zip tool found. Install zip, 7-Zip, or PowerShell." >&2
    exit 1
  fi

  if [ ! -f "$zip_path" ]; then
    echo "Error: expected archive was not created: $zip_path" >&2
    exit 1
  fi
}

build_macos() {
  local arch target_dir binary version version_dir release_dir

  chmod +x build.sh
  ./build.sh release

  binary="${BUILD_DIR}/cksp"
  if [ ! -x "$binary" ]; then
    echo "Error: expected binary not found: $binary" >&2
    exit 1
  fi

  version=$("$binary" --version | awk '{print $3}')
  if [ -z "$version" ]; then
    echo "Error: could not determine CKSP version." >&2
    exit 1
  fi

  if [[ "$version" == *"-"* ]]; then
    version_dir="cksp_v${version}"
  else
    version_dir="cksp_v${version}_release"
  fi

  arch=$(uname -m)
  if [ "$arch" = "arm64" ]; then
    target_dir="macos_arm64"
  else
    target_dir="macos_x86_64"
  fi

  release_dir="${RELEASES_DIR}/${version_dir}"
  prepare_release_dir "$release_dir"
  "$binary" --help > "$release_dir/readme.txt"
  cp "$binary" "$release_dir/${target_dir}/cksp"
  chmod +x "$release_dir/${target_dir}/cksp"

  create_zip "$version_dir"

  VERSION="$version"
  VERSION_DIR="$version_dir"
}

build_windows() {
  cmd.exe /c build.bat

  local binary version version_dir release_dir
  binary="${BUILD_DIR}/cksp.exe"
  if [ ! -f "$binary" ]; then
    echo "Error: expected binary not found: $binary" >&2
    exit 1
  fi

  version=$("$binary" --version | awk '{print $3}')
  if [ -z "$version" ]; then
    echo "Error: could not determine CKSP version." >&2
    exit 1
  fi

  if [[ "$version" == *"-"* ]]; then
    version_dir="cksp_v${version}"
  else
    version_dir="cksp_v${version}_release"
  fi

  release_dir="${RELEASES_DIR}/${version_dir}"
  prepare_release_dir "$release_dir"
  "$binary" --help > "$release_dir/readme.txt"
  cp "$binary" "$release_dir/windows/cksp.exe"

  create_zip "$version_dir"

  VERSION="$version"
  VERSION_DIR="$version_dir"
}

require_command awk
require_command gh
require_command git

PLATFORM=$(detect_platform)
case "$PLATFORM" in
  macos)
    build_macos
    ;;
  windows)
    build_windows
    ;;
  *)
    echo "Error: local publishing is currently supported on macOS and Windows only." >&2
    exit 1
    ;;
esac

TAG="v${VERSION}"
ARCHIVE_PATH="${RELEASES_DIR}/${VERSION_DIR}.zip"
IS_PRERELEASE="false"
if [[ "$TAG" == *"-"* ]]; then
  IS_PRERELEASE="true"
fi

if [ -f CHANGELOG.md ]; then
  NOTES_FILE=$(mktemp)
  trap 'rm -f "$NOTES_FILE"' EXIT
  cp CHANGELOG.md "$NOTES_FILE"
else
  NOTES_FILE=$(mktemp)
  trap 'rm -f "$NOTES_FILE"' EXIT
  echo "Automatically generated release for ${TAG}" > "$NOTES_FILE"
fi

echo "Publishing ${TAG} to ${REPO}"
echo "Archive: ${ARCHIVE_PATH}"

if gh release view "$TAG" --repo "$REPO" >/dev/null 2>&1; then
  confirm_replace
  gh release delete "$TAG" --repo "$REPO" --yes
fi

if git rev-parse "$TAG" >/dev/null 2>&1; then
  confirm_replace
  git tag -d "$TAG"
fi

if git ls-remote --exit-code --tags origin "refs/tags/${TAG}" >/dev/null 2>&1; then
  confirm_replace
  git push --delete origin "$TAG"
fi

git tag -a "$TAG" -m "Release $TAG"
git push origin "$TAG"

gh release create "$TAG" \
  "$ARCHIVE_PATH" \
  --repo "$REPO" \
  --title "$TAG" \
  --notes-file "$NOTES_FILE" \
  --prerelease="$IS_PRERELEASE"

echo "Published ${TAG} to ${REPO}."
