SUBMODULE_DIR="cksp-compiler-issues"
CHANGELOG_DIR="${SUBMODULE_DIR}/changelogs"
SUBMODULE_BRANCH="cksp-releases"

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
# Zurück ins ursprüngliche Verzeichnis
popd >/dev/null