#!/usr/bin/env bash

# Intentionally no 'set -e': we want to aggregate pass/fail per test.

# -----------------------------
# Static config (adjust if needed)
# -----------------------------
BASE_DIR="$(pwd)"
DEBUG_EXEC="$BASE_DIR/cmake-build-debug/cksp"
RELEASE_EXEC="$BASE_DIR/cmake-build-release/cksp"

# Kontakt toggle flags
USE_KONTAKT=false        # compile + kontakt (enable via --with-kontakt)
# KONTAKT_ONLY=false       # kontakt only (enable via --kontakt-only)

# Kontakt executable and Python runner
KONTAKT_EXEC="/Applications/Native Instruments/Kontakt 7/Kontakt 7.app/Contents/macOS/Kontakt 7"
KONTAKT_RUNNER="$BASE_DIR/tests/resources/run_ksp_script.py"

# Runner tuning (not exposed via CLI; change here if needed)
IDLE="0.5"
STARTUP_GRACE="3.5"
HARD_TIMEOUT="30"
GRACE="2.0"

GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[0;33m'; RESET='\033[0m'

# -----------------------------
# Default test files (extend with extra CLI args)
# -----------------------------
FILES=(
  "/Users/mathias/Scripting/the-score/the-score.ksp"
  "/Users/Mathias/Scripting/the-pulse/the-pulse.ksp"
  "/Users/mathias/Scripting/lux-strings/dev/Lux - Orchestral Strings Keyswitch.ksp"
  "/Users/mathias/Scripting/time-textures/time-textures.ksp"
  "/Users/mathias/Scripting/the-orchestra-complete-4/the_orchestra_ens_V1.2.ksp"
  "/Users/mathias/Scripting/legato-dev/legato.ksp"
  "/Users/mathias/Scripting/legato-dev/keyswitch.ksp"
  "/Users/mathias/Scripting/fluegel/fluegel.ksp"
  "/Users/mathias/Scripting/chroma-upright-piano/Chroma Cimbalom.ksp"
  "/Users/mathias/Scripting/ro-ki/rho_des.ksp"
  "/Users/mathias/Scripting/pipe-organ/pipe-organ.ksp"
  "/Users/Mathias/Scripting/the-score/the-score-lead.ksp"
  "/Users/Mathias/Scripting/action-woodwinds/action-ww.ksp"
  "/Users/Mathias/Scripting/action-strings-2/action_strings2_V0.1.ksp"
  "/Users/Mathias/Scripting/horizon-leads/Horizon Leads.ksp"
  "/Users/mathias/Scripting/fragments-modern-percussion/Fragments.ksp"
  "/Users/mathias/Scripting/sonu-northern-spheres/Nordic Spheres.ksp"
)

# -----------------------------
# CLI parsing
# -----------------------------
while [[ $# -gt 0 ]]; do
  case "$1" in
	--with-kontakt) USE_KONTAKT=true; shift;;
	*)               FILES+=("$1");      shift;;
  esac
done

# if [[ "$USE_KONTAKT" == true && "$KONTAKT_ONLY" == true ]]; then
#   echo "❗️ Options --with-kontakt and --kontakt-only are mutually exclusive."
#   exit 2
# fi

# -----------------------------
# Log root
# -----------------------------
LOG_ROOT="$BASE_DIR/tests/logs"
mkdir -p "$LOG_ROOT"

# -----------------------------
# Spinner helpers
# -----------------------------
start_spinner() {
  local label="$1"
  local spin_chars='-\|/'
  local i=0
  tput civis 2>/dev/null || true
  while true; do
	i=$(( (i+1) % 4 ))
	printf "\r⏳ %s ... %s " "$label" "${spin_chars:$i:1}"
	sleep 0.1
  done
}

stop_spinner() {
  kill "$1" &>/dev/null || true
  wait "$1" 2>/dev/null || true
  tput cnorm 2>/dev/null || true
  printf "\r"
}

# High-resolution timestamp in milliseconds (portable for macOS/Linux)
now_ms() {
  if command -v python3 >/dev/null 2>&1; then
    python3 -c 'import time; print(int(time.time() * 1000))'
  elif command -v perl >/dev/null 2>&1; then
    perl -MTime::HiRes=time -e 'print int(time() * 1000)'
  else
    echo $(( $(date +%s) * 1000 ))
  fi
}

# -----------------------------
# Compile (default) + optional Kontakt
# -----------------------------
BUILDS=(
  # "debug:$DEBUG_EXEC"
  "release:$RELEASE_EXEC"
)

echo "🚀 Starting CKSP Tests (Kontakt runner: $([[ "$USE_KONTAKT" == true ]] && echo ON || echo OFF))"
echo "================================================================================"

for entry in "${BUILDS[@]}"; do
  mode="${entry%%:*}"
  executable="${entry#*:}"

  ./build.sh "$mode"

  echo ""
  echo "🔧 Testing [$mode] build: $executable"
  echo "-------------------------------------"

  if [[ ! -x "$executable" ]]; then
	echo "❗️ Executable not found or not executable: $executable"
	continue
  fi

  if [[ "$USE_KONTAKT" == true ]]; then
	if [[ ! -x "$KONTAKT_EXEC" ]]; then
	  echo "❗️ Kontakt executable not found or not executable: $KONTAKT_EXEC"
	  echo "    Disable runner or fix KONTAKT_EXEC path in this script."
	  exit 127
	fi
	if [[ ! -f "$KONTAKT_RUNNER" ]]; then
	  echo "❗️ Python Kontakt runner not found: $KONTAKT_RUNNER"
	  echo "    Disable runner or fix KONTAKT_RUNNER path in this script."
	  exit 2
	fi
  fi

  VERSION_OUTPUT="$("$executable" --version 2>/dev/null || true)"
  VERSION=$(echo "$VERSION_OUTPUT" | grep -oE '[0-9]+\.[0-9]+\.[0-9]+(-[a-z0-9.]+)?' || true)
  if [[ -z "$VERSION" ]]; then
	echo "❗️Could not detect version of $executable – skipping tests"
	continue
  fi
  echo "   ➤ Version: $VERSION"

  compile_passed=0; compile_failed=0
  kontakt_passed=0; kontakt_failed=0
  kontakt_skipped=0
  failed_compile_list=()
  failed_kontakt_list=()

  for file in "${FILES[@]}"; do
	filename=$(basename "$file")
	filename="${filename%.ksp}"
	filename="${filename%.cksp}"

	log_dir="$LOG_ROOT/$mode/$VERSION/$filename"
	rm -rf "$log_dir"
	mkdir -p "$log_dir"

	stdout_log="$log_dir/stdout.log"
	stderr_log="$log_dir/stderr.log"
	kontakt_log="$log_dir/kontakt_output.log"
	OUTPUT_FILE="$log_dir/code_output.txt"

		# ----- Compile -----
		label="compile: $filename"
		start_spinner "$label" & spin_pid=$!
		start_ms=$(now_ms)

		"$executable" -o "$OUTPUT_FILE" "$file" >"$log_dir/.tmp_compile" 2>&1
		compile_exit=$?

		stop_spinner "$spin_pid"
		end_ms=$(now_ms)
		duration_compile_ms=$(( end_ms - start_ms ))
		duration_compile=$(awk -v ms="$duration_compile_ms" 'BEGIN { printf "%.3f", ms / 1000 }')

	sed -E 's/\x1B\[[0-9;]*[mK]//g' "$log_dir/.tmp_compile" > "$log_dir/.compile_clean"
	rm -f "$log_dir/.tmp_compile"

	if [[ $compile_exit -eq 0 ]]; then
		  echo -e "✅ ${GREEN}Compiled${RESET} (${duration_compile} s) - $filename"
	  compile_passed=$((compile_passed + 1))
	  mv "$log_dir/.compile_clean" "$stdout_log"
	else
		  echo -e "❌ ${RED}Compile failed${RESET} (${duration_compile} s) - $filename"
	  compile_failed=$((compile_failed + 1))
	  failed_compile_list+=("$filename")
	  mv "$log_dir/.compile_clean" "$stderr_log"

	  echo "    ↪ compile errors (excerpt):"
	  grep -E 'CompileError|error|ERROR' "$stderr_log" | head -n 20 | sed 's/^/        /' || true
	  printf '\033[0m'
	  if [[ "$USE_KONTAKT" == true ]]; then
		kontakt_skipped=$((kontakt_skipped + 1))
	  fi
	  continue
	fi

	# ----- Kontakt (optional) -----
		if [[ "$USE_KONTAKT" == true ]]; then
		  label="kontakt: $filename"
		  start_spinner "$label" & spin2_pid=$!

		  start_ms=$(now_ms)
		  python3 "$KONTAKT_RUNNER" \
			--kontakt "$KONTAKT_EXEC" \
			--idle "$IDLE" \
		--startup-grace "$STARTUP_GRACE" \
		--hard-timeout "$HARD_TIMEOUT" \
		--grace "$GRACE" \
		--output "$kontakt_log" \
		"$OUTPUT_FILE"
		  kontakt_exit=$?

		  stop_spinner "$spin2_pid"
		  end_ms=$(now_ms)
		  duration_kontakt_ms=$(( end_ms - start_ms ))
		  duration_kontakt=$(awk -v ms="$duration_kontakt_ms" 'BEGIN { printf "%.3f", ms / 1000 }')

	  if [[ $kontakt_exit -eq 1 ]]; then
		echo -e "❌ ${RED}Kontakt failed${RESET} ($duration_kontakt s) - $filename (rc=$kontakt_exit)"
		kontakt_failed=$((kontakt_failed + 1))
		failed_kontakt_list+=("$filename")

		echo "    ↪ kontakt errors (excerpt):"
		grep -i "\[error\]\|error\|failed" "$kontakt_log" | head -n 1 | sed 's/^/        /' || true
		printf '\033[0m'
	  else
		echo -e "✅ ${GREEN}Kontakt OK${RESET} ($duration_kontakt s) - $filename"
		kontakt_passed=$((kontakt_passed + 1))
	  fi
	fi
  done

  echo "-------------------------------------"
  echo "📦 [$mode/$VERSION] Summary:"
  echo "   🧱 Compile:  ✅ ${compile_passed}   ❌ ${compile_failed}"
  if [[ "$USE_KONTAKT" == true ]]; then
	echo "   🎹 Kontakt:  ✅ ${kontakt_passed}   ❌ ${kontakt_failed}   ⏭️ skipped: ${kontakt_skipped}"
  else
	echo "   🎹 Kontakt:  (runner disabled)"
  fi
  echo "   📁 Logs:     $LOG_ROOT/$mode/$VERSION"
  if (( ${#failed_compile_list[@]} )); then
	echo -e "   ${YELLOW}Failed (compile):${RESET} ${failed_compile_list[*]}"
  fi
  if (( ${#failed_kontakt_list[@]} )); then
	echo -e "   ${YELLOW}Failed (kontakt):${RESET} ${failed_kontakt_list[*]}"
  fi
done
