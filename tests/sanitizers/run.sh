#!/usr/bin/env bash

# Memory safety of the compiler itself.
#
# Compiles every .ksp / .cksp file under tests/ with an Address/UndefinedBehaviour sanitizer build
# and fails on any report. The bugs this catches do not show up as a failed compile: the AST is built from
# raw back pointers between nodes, and reading through a stale one usually lands on memory that
# still looks plausible. The compiler then emits the right output almost every time and fails at
# random on the rest, depending on what the allocator put in the freed block - see
# tests/resources/ndarray_member_ref_counting.cksp, which used to fail this way in roughly a third
# of its runs while producing byte-identical output whenever it succeeded.
#
# Only tests/ is covered by default, so the suite runs anywhere. The project corpus of
# run_tests.sh is clean as well and can be handed in as extra arguments:
#
#   tests/sanitizers/run.sh "" ~/Scripting/the-score/the-score.ksp ~/Scripting/fluegel/fluegel.ksp
#
# Usage: tests/sanitizers/run.sh [path/to/sanitizer-cksp] [extra source files...]
#
# Without a binary an Address/UndefinedBehaviour build is configured in cmake-build-asan/.

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_DIR="$(cd "$DIR/../.." && pwd)"
EXEC="${1:-}"
shift || true
EXTRA_FILES=("$@")

GREEN='\033[0;32m'; RED='\033[0;31m'; DIM='\033[2m'; RESET='\033[0m'

if [[ -z "$EXEC" ]]; then
  EXEC="$BASE_DIR/cmake-build-asan/cksp"
  echo "⚙️  building an Address/UndefinedBehaviour cksp in cmake-build-asan ..."
  cmake -S "$BASE_DIR" -B "$BASE_DIR/cmake-build-asan" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" >/dev/null || exit $?
  cmake --build "$BASE_DIR/cmake-build-asan" --target cksp -j 8 >/dev/null || exit $?
fi

if [[ ! -x "$EXEC" ]]; then
  echo "❗️ Executable not found or not executable: $EXEC"
  exit 127
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

passed=0
failed=0

# leak detection is off: the AST holds deliberate cycles and static visitors keep nodes alive
# past main(), neither of which is what this suite is about
export ASAN_OPTIONS="detect_leaks=0"

while IFS= read -r src; do
  name="$(basename "$src")"
  log="$TMP/$name.log"

  "$EXEC" -o "$TMP/$name.txt" "$src" >"$log" 2>&1
  report="$(grep -E "ERROR: (AddressSanitizer|LeakSanitizer)|runtime error:" "$log" | head -3)"

  if [[ -n "$report" ]]; then
    echo -e "${RED}❌ $name${RESET}"
    echo "$report" | sed 's/^/     /'
    echo -e "     ${DIM}$(grep -E "^    #[0-9]+ 0x" "$log" | head -3 | sed 's/^ *//')${RESET}"
    ((failed++))
  else
    echo -e "${GREEN}✅ $name${RESET}"
    ((passed++))
  fi
done < <({ find "$BASE_DIR/tests" -name '*.ksp' -o -name '*.cksp' | sort
           (( ${#EXTRA_FILES[@]} )) && printf '%s\n' "${EXTRA_FILES[@]}"; })

echo "-------------------------------------"
echo -e "🧪 Sanitizers: ${GREEN}✅ $passed${RESET}   ${RED}❌ $failed${RESET}"
[[ $failed -eq 0 ]]
