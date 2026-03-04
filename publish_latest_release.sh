#!/bin/bash

# === Configuration ===
PRIVATE_REPO="mathiasvatter/cksp-compiler"
PUBLIC_REPO="mathiasvatter/cksp-compiler-issues" # Your public repo
SUBMODULE_DIR="cksp-compiler-issues" # Local folder name of the submodule
# === End configuration ===

# Error handling function
check_error() {
  if [ $? -ne 0 ]; then
    echo "Error: $1 failed." >&2
    # Optional: clean up temporary directory if it exists
    if [ -d "$TEMP_ASSET_DIR" ]; then
      rm -rf "$TEMP_ASSET_DIR"
    fi
    exit 1
  fi
}

echo "Starting release publication from private to public repo..."

# 1. Find the latest release in the private repo
echo "Fetching latest release tag from private repo ($PRIVATE_REPO)..."
LATEST_PRIVATE_RELEASE_INFO_JSON=$(gh release list --repo "$PRIVATE_REPO" --limit 1 --json tagName,isPrerelease)
check_error "Fetching latest release info"

if [ -z "$LATEST_PRIVATE_RELEASE_INFO_JSON" ] || [ "$(echo "$LATEST_PRIVATE_RELEASE_INFO_JSON" | jq 'length')" -eq 0 ]; then
    echo "Error: No releases found in private repository $PRIVATE_REPO." >&2
    exit 1
fi

LATEST_PRIVATE_TAG=$(echo "$LATEST_PRIVATE_RELEASE_INFO_JSON" | jq -r '.[0].tagName')
# IS_LATEST_PRERELEASE=$(echo "$LATEST_PRIVATE_RELEASE_INFO_JSON" | jq -r '.[0].isPrerelease') # Retrieved again below

if [ -z "$LATEST_PRIVATE_TAG" ]; then
    echo "Error: Could not extract latest tag name from private repo." >&2
    exit 1
fi
echo "Latest private release tag found: $LATEST_PRIVATE_TAG"

# 2. Fetch all details for this release
echo "Fetching details for release $LATEST_PRIVATE_TAG from $PRIVATE_REPO..."
RELEASE_DETAILS_JSON=$(gh release view "$LATEST_PRIVATE_TAG" --repo "$PRIVATE_REPO" --json tagName,name,body,isPrerelease,assets)
check_error "Fetching release details"

# Parse details
TAG=$(echo "$RELEASE_DETAILS_JSON" | jq -r '.tagName')
# Use release name, fall back to tag if no name is set
RELEASE_NAME=$(echo "$RELEASE_DETAILS_JSON" | jq -r '.name // .tagName')
BODY=$(echo "$RELEASE_DETAILS_JSON" | jq -r '.body')
IS_PRERELEASE=$(echo "$RELEASE_DETAILS_JSON" | jq -r '.isPrerelease')

PRE_RELEASE_FLAG=""
if [ "$IS_PRERELEASE" == "true" ]; then
    PRE_RELEASE_FLAG="--prerelease"
    echo "This is a pre-release."
else
    echo "This is a stable release."
fi

# Extract asset names
ASSET_NAMES=()
while IFS= read -r line; do
    ASSET_NAMES+=("$line")
done < <(echo "$RELEASE_DETAILS_JSON" | jq -r '.assets[].name')

if [ ${#ASSET_NAMES[@]} -eq 0 ]; then
    echo "Error: No assets found for release $TAG in $PRIVATE_REPO." >&2
    exit 1
fi
echo "Found assets: ${ASSET_NAMES[*]}"

# 3. Download assets
TEMP_ASSET_DIR=$(mktemp -d)
echo "Created temporary directory for assets: $TEMP_ASSET_DIR"
# Ensure the temporary directory is deleted when the script ends
trap 'echo "Cleaning up temporary directory..."; rm -rf "$TEMP_ASSET_DIR"' EXIT

ASSET_PATHS=()
echo "Downloading assets..."
for ASSET_NAME in "${ASSET_NAMES[@]}"; do
    echo "Downloading $ASSET_NAME..."
    gh release download "$TAG" --repo "$PRIVATE_REPO" --pattern "$ASSET_NAME" --dir "$TEMP_ASSET_DIR" --clobber
    check_error "Downloading asset $ASSET_NAME"
    ASSET_PATHS+=("$TEMP_ASSET_DIR/$ASSET_NAME")
done
echo "Assets downloaded successfully."

# 5. Prepare public repo (delete tag + release, create new tag)
echo "Preparing public repository ($PUBLIC_REPO)..."

# Switch to submodule directory
pushd "$SUBMODULE_DIR" >/dev/null
check_error "Changing into submodule directory $SUBMODULE_DIR"

# Delete old tag (local and remote) if it exists
if git rev-parse "$TAG" >/dev/null 2>&1; then
    echo "Tag '$TAG' exists locally in submodule. Deleting..."
    git tag -d "$TAG"
    check_error "Deleting local tag"
    # Also try deleting remotely (may fail if not present, ignore)
    git push --delete origin "$TAG" 2>/dev/null || true
fi

# Delete old GitHub release in the public repo if it exists
if gh release view "$TAG" --repo "$PUBLIC_REPO" >/dev/null 2>&1; then
    echo "Release '$TAG' exists on public GitHub repo. Deleting..."
    gh release delete "$TAG" --repo "$PUBLIC_REPO" --yes
    check_error "Deleting existing public GitHub release"
fi

# Create new tag in submodule (points to current submodule HEAD) and push it
echo "Creating and pushing new tag '$TAG' for submodule..."
git tag "$TAG"
check_error "Creating local tag"
git push origin "$TAG"
check_error "Pushing tag to origin"

# Return to the original directory
popd >/dev/null

# 6. Create new release in the public repo
echo "Creating new release '$RELEASE_NAME' ($TAG) on public repo $PUBLIC_REPO..."

# The command accepts all downloaded asset paths
gh release create "$TAG" \
  --repo "$PUBLIC_REPO" \
  --title "$RELEASE_NAME" \
  --notes "$BODY" \
  $PRE_RELEASE_FLAG \
  "${ASSET_PATHS[@]}" # Adds all paths from the array as arguments

check_error "Creating new public GitHub release"

echo "Successfully published release $TAG from $PRIVATE_REPO to $PUBLIC_REPO."
echo "Assets uploaded: ${ASSET_NAMES[*]}"

# Cleanup happens automatically via the 'trap' command at the end
exit 0
