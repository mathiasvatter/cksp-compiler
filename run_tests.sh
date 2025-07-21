#!/usr/bin/env bash

# Paths to your compiler executables
BASE_DIR="$(pwd)"
DEBUG_EXEC="$BASE_DIR/cmake-build-debug/cksp"
RELEASE_EXEC="$BASE_DIR/cmake-build-release/cksp"
GREEN='\033[0;32m'
RED='\033[0;31m'
RESET='\033[0m'

# Default test files (hardcoded)
FILES=(
  "/Users/mathias/Scripting/the-score/the-score.ksp"
  "/Users/Mathias/Scripting/the-pulse/the-pulse.ksp"
  "/Users/mathias/Scripting/lux-strings/dev/Lux - Orchestral Strings Keyswitch.ksp"
  "/Users/mathias/Scripting/time-textures/time-textures.ksp"
  "/Users/mathias/Scripting/legato-dev/legato.ksp"
  "/Users/mathias/Scripting/legato-dev/keyswitch.ksp"
  "/Users/mathias/Scripting/ro-ki/rho_des.ksp"
  "/Users/mathias/Scripting/pipe-organ/pipe-organ.ksp"
# "/Users/mathias/Scripting/preset-system/main.ksp"
  "/Users/Mathias/Scripting/the-score/the-score-lead.ksp"
  "/Users/Mathias/Scripting/action-woodwinds/action-ww.ksp"
  "/Users/Mathias/Scripting/action-strings-2/action_strings2_V0.1.ksp"
  "/Users/Mathias/Scripting/horizon-leads/Horizon Leads.ksp"
)

# Append additional files from command-line arguments (if any)
for arg in "$@"; do
  FILES+=("$arg")
done

# Root directory for log files
LOG_ROOT="$BASE_DIR/test_logs"
mkdir -p "$LOG_ROOT"
OUTPUT_FILE="$LOG_ROOT/cksp_tests_out.txt"

# Build configurations and corresponding executables
BUILDS=(
#  "debug:$DEBUG_EXEC"
  "release:$RELEASE_EXEC"
)

# Spinner-Funktionen
start_spinner() {
  local spin_chars='-\|/'
  i=0
  tput civis  # Cursor ausblenden
  while true; do
    i=$(( (i+1) % 4 ))
    printf "\r⏳ $CURRENT_FILE ... ${spin_chars:$i:1} "
    sleep 0.1
  done
}

stop_spinner() {
  kill "$1" &>/dev/null
  wait "$1" 2>/dev/null
  tput cnorm  # Cursor wieder einblenden
}


echo "🚀 Starting CKSP Tests..."
echo "============================="

for entry in "${BUILDS[@]}"; do
  mode="${entry%%:*}"
  executable="${entry#*:}"

  ./build.sh "$mode"

  echo ""
  echo "🔧 Testing [$mode] build: $executable"
  echo "-----------------------------"

  if [[ ! -x "$executable" ]]; then
    echo "❗️ Executable not found or not executable: $executable"
    continue
  fi

  # Extract compiler version
  VERSION_OUTPUT="$("$executable" --version 2>/dev/null)"
  VERSION=$(echo "$VERSION_OUTPUT" | grep -oE '[0-9]+\.[0-9]+\.[0-9]+(-[a-z0-9.]+)?')
  if [[ -z "$VERSION" ]]; then
    echo "❗️Could not detect version of $executable – skipping tests"
    continue
  fi
  echo "   ➤ Version: $VERSION"

  passed=0
  failed=0

  for file in "${FILES[@]}"; do
    filename=$(basename "$file")
    filename="${filename%.ksp}"
    filename="${filename%.cksp}"
    log_dir="$LOG_ROOT/$mode/$VERSION/$filename"
    rm -rf "$log_dir"
    mkdir -p "$log_dir"
    stdout_log="$log_dir/stdout.log"
    stderr_log="$log_dir/stderr.log"

#    echo -n "⏳ $filename ... "
    # Aktuellen Dateinamen für Spinner anzeigen
    CURRENT_FILE="$filename"

    # Start time in ms
    start_time=$(date +%s)

    # Start Spinner
    start_spinner &
    spinner_pid=$!
    OUTPUT_FILE="$log_dir/code_output.txt"

    # Run and capture output
    "$executable" -o "$OUTPUT_FILE" "$file" >"$log_dir/.tmp_stdout" 2>&1
    exit_code=$?

    # Stop Spinner
    stop_spinner "$spinner_pid"

    end_time=$(date +%s)
    duration_sec=$((end_time-start_time))
    duration_sec=${duration_sec%.*}

    if [[ $exit_code -eq 0 ]]; then
      echo "✅ Passed (${duration_sec} sec)"
      passed=$((passed + 1))
    else
      echo "❌ Failed (${duration_sec} sec)"
      failed=$((failed + 1))
      echo "    ↪ stderr:"
      grep --color=always 'CompileError' "$log_dir/.tmp_stdout" \
        | awk '/^Seems like the compilation/ { exit } { print }' \
        | sed 's/^/        /'

      # Reset ANSI colors to avoid color bleed into next output
      printf '\033[0m'

    fi

    # Remove ANSI color codes and save logfiles
    sed -E 's/\x1B\[[0-9;]*[mK]//g' "$log_dir/.tmp_stdout" > "$log_dir/.clean_log"
    rm -f "$log_dir/.tmp_stdout"

    # Store cleaned log
    if [[ $exit_code -eq 0 ]]; then
      mv "$log_dir/.clean_log" "$stdout_log"
    else
      mv "$log_dir/.clean_log" "$stderr_log"
    fi
  done

  echo "-----------------------------"
  echo "📦 [$mode/$VERSION] Summary:"
  echo "   ✅ Passed: $passed"
  echo "   ❌ Failed: $failed"
  echo "   📁 Logs: $LOG_ROOT/$mode/$VERSION"
done

rm "$OUTPUT_FILE"





