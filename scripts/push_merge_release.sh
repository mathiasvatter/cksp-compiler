#!/bin/bash

# Speichere den aktuellen Branch
CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
BUILD_DIR="cmake-build-release"
# Execute cksp --version and extract the version number
VERSION=$("$BUILD_DIR/cksp" --version | awk '{print $3}')

# Überprüfe auf uncommitted changes
if git diff-index --quiet HEAD --; then
    echo "No changes to commit."
else
    # Füge alle Änderungen hinzu und committe sie mit der Nachricht "Update executables"
    git add .
    git commit -m "Update executables for version $VERSION."
    echo "Changes committed with message: 'Update executables'."
fi

# Pushe die Änderungen des aktuellen Branches
echo "Pushing changes to the remote branch '$CURRENT_BRANCH'..."
git push origin "$CURRENT_BRANCH"

# Wechsel zum master-Branch
echo "Switching to master branch..."
git checkout master

# Versuche den aktuellen Branch in den master zu mergen
echo "Merging '$CURRENT_BRANCH' into master..."
if git merge "$CURRENT_BRANCH"; then
    echo "Merge successful."
else
    # Bei Merge-Konflikten
    echo "Error: Merge conflict occurred while merging '$CURRENT_BRANCH' into 'master'. Please resolve the conflicts and try again." >&2
    exit 1
fi

# Pushe den Merge auf den master-Branch
echo "Pushing merged changes to master..."
git push origin master

echo "Merge and push completed successfully."
