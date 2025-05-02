#!/bin/bash

# === Konfiguration ===
PRIVATE_REPO="mathiasvatter/cksp-compiler"
PUBLIC_REPO="mathiasvatter/cksp-compiler-issues" # Dein öffentliches Repo
SUBMODULE_DIR="cksp-compiler-issues" # Der lokale Ordnername des Submodules
# === Ende Konfiguration ===

# Funktion für Fehlerbehandlung
check_error() {
  if [ $? -ne 0 ]; then
    echo "Error: $1 failed." >&2
    # Optional: Temp-Verzeichnis aufräumen, falls es existiert
    if [ -d "$TEMP_ASSET_DIR" ]; then
      rm -rf "$TEMP_ASSET_DIR"
    fi
    exit 1
  fi
}

echo "Starting release publication from private to public repo..."

# 1. Finde das neueste Release im privaten Repo
echo "Fetching latest release tag from private repo ($PRIVATE_REPO)..."
LATEST_PRIVATE_RELEASE_INFO_JSON=$(gh release list --repo "$PRIVATE_REPO" --limit 1 --json tagName,isPrerelease)
check_error "Fetching latest release info"

if [ -z "$LATEST_PRIVATE_RELEASE_INFO_JSON" ] || [ "$(echo "$LATEST_PRIVATE_RELEASE_INFO_JSON" | jq 'length')" -eq 0 ]; then
    echo "Error: No releases found in private repository $PRIVATE_REPO." >&2
    exit 1
fi

LATEST_PRIVATE_TAG=$(echo "$LATEST_PRIVATE_RELEASE_INFO_JSON" | jq -r '.[0].tagName')
# IS_LATEST_PRERELEASE=$(echo "$LATEST_PRIVATE_RELEASE_INFO_JSON" | jq -r '.[0].isPrerelease') # Wird unten nochmal geholt

if [ -z "$LATEST_PRIVATE_TAG" ]; then
    echo "Error: Could not extract latest tag name from private repo." >&2
    exit 1
fi
echo "Latest private release tag found: $LATEST_PRIVATE_TAG"

# 2. Hole alle Details zu diesem Release
echo "Fetching details for release $LATEST_PRIVATE_TAG from $PRIVATE_REPO..."
RELEASE_DETAILS_JSON=$(gh release view "$LATEST_PRIVATE_TAG" --repo "$PRIVATE_REPO" --json tagName,name,body,isPrerelease,assets)
check_error "Fetching release details"

# Parse die Details
TAG=$(echo "$RELEASE_DETAILS_JSON" | jq -r '.tagName')
# Verwende den Release-Namen, falle auf Tag zurück, wenn kein Name gesetzt ist
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

# Extrahiere Asset-Namen
ASSET_NAMES=()
while IFS= read -r line; do
    ASSET_NAMES+=("$line")
done < <(echo "$RELEASE_DETAILS_JSON" | jq -r '.assets[].name')

if [ ${#ASSET_NAMES[@]} -eq 0 ]; then
    echo "Error: No assets found for release $TAG in $PRIVATE_REPO." >&2
    exit 1
fi
echo "Found assets: ${ASSET_NAMES[*]}"

# 3. Assets herunterladen
TEMP_ASSET_DIR=$(mktemp -d)
echo "Created temporary directory for assets: $TEMP_ASSET_DIR"
# Stelle sicher, dass das Temp-Verzeichnis bei Skriptende gelöscht wird
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

# 4. (Optional) Changelogs pushen (wenn nötig)
echo "Pushing potential changelog updates..."
./scripts/push_changelog.sh
check_error "Pushing changelogs"

# 5. Öffentliches Repo vorbereiten (Tag + Release löschen, neuen Tag erstellen)
echo "Preparing public repository ($PUBLIC_REPO)..."

# Wechsle ins Submodule-Verzeichnis
pushd "$SUBMODULE_DIR" >/dev/null
check_error "Changing into submodule directory $SUBMODULE_DIR"

# Lösche den alten Tag (lokal und remote), falls er existiert
if git rev-parse "$TAG" >/dev/null 2>&1; then
    echo "Tag '$TAG' exists locally in submodule. Deleting..."
    git tag -d "$TAG"
    check_error "Deleting local tag"
    # Versuche auch remote zu löschen (kann fehlschlagen, wenn nicht vorhanden, ignorieren)
    git push --delete origin "$TAG" 2>/dev/null || true
fi

# Lösche das alte GitHub Release im öffentlichen Repo, falls es existiert
if gh release view "$TAG" --repo "$PUBLIC_REPO" >/dev/null 2>&1; then
    echo "Release '$TAG' exists on public GitHub repo. Deleting..."
    gh release delete "$TAG" --repo "$PUBLIC_REPO" --yes
    check_error "Deleting existing public GitHub release"
fi

# Erstelle den neuen Tag im Submodule (zeigt auf aktuellen HEAD des Submodules) und pushe ihn
echo "Creating and pushing new tag '$TAG' for submodule..."
git tag "$TAG"
check_error "Creating local tag"
git push origin "$TAG"
check_error "Pushing tag to origin"

# Zurück ins ursprüngliche Verzeichnis
popd >/dev/null

# 6. Neues Release im öffentlichen Repo erstellen
echo "Creating new release '$RELEASE_NAME' ($TAG) on public repo $PUBLIC_REPO..."

# Der Befehl nimmt alle heruntergeladenen Asset-Pfade entgegen
gh release create "$TAG" \
  --repo "$PUBLIC_REPO" \
  --title "$RELEASE_NAME" \
  --notes "$BODY" \
  $PRE_RELEASE_FLAG \
  "${ASSET_PATHS[@]}" # Fügt alle Pfade aus dem Array als Argumente hinzu

check_error "Creating new public GitHub release"

echo "Successfully published release $TAG from $PRIVATE_REPO to $PUBLIC_REPO."
echo "Assets uploaded: ${ASSET_NAMES[*]}"

# Cleanup passiert automatisch durch den 'trap' Befehl am Ende
exit 0