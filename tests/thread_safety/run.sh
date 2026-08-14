#!/usr/bin/env bash

# Thread safety / dimension expansion regression tests.
#
# Each .ksp file in this directory carries its expectations as comments:
#
#   // EXPECT     <extended regex>   -> must match somewhere in the generated KSP
#   // EXPECT_NOT <extended regex>   -> must not match anywhere in the generated KSP
#
# Generated identifiers get a gensym suffix, so the patterns match on the shape of a declaration
# (name prefix plus the expanded size) rather than on a literal name. The callback depth is pinned
# so <1000> stays the expanded size regardless of the default.
#
# Usage: tests/thread_safety/run.sh [path/to/cksp]

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_DIR="$(cd "$DIR/../.." && pwd)"
EXEC="${1:-$BASE_DIR/cmake-build-release/cksp}"
DEPTH=1000

GREEN='\033[0;32m'; RED='\033[0;31m'; DIM='\033[2m'; RESET='\033[0m'

if [[ ! -x "$EXEC" ]]; then
  echo "❗️ Executable not found or not executable: $EXEC"
  exit 127
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

passed=0
failed=0

for src in "$DIR"/*.ksp; do
  name="$(basename "$src" .ksp)"
  out="$TMP/$name.txt"

  if ! "$EXEC" -o "$out" -s "$DEPTH" "$src" >"$TMP/$name.log" 2>&1; then
    echo -e "${RED}❌ $name${RESET} - compile failed"
    sed 's/^/     /' "$TMP/$name.log" | tail -5
    ((failed++))
    continue
  fi

  file_failed=0
  assertions=0
  while IFS= read -r assertion; do
    ((assertions++))
    [[ -z "$assertion" ]] && continue
    kind="${assertion%% *}"
    pattern="${assertion#"$kind" }"

    if grep -qE -- "$pattern" "$out"; then found=true; else found=false; fi

    case "$kind" in
      EXPECT)
        if [[ "$found" == false ]]; then
          echo -e "${RED}❌ $name${RESET} - expected a match for: $pattern"
          file_failed=1
        fi
        ;;
      EXPECT_NOT)
        if [[ "$found" == true ]]; then
          echo -e "${RED}❌ $name${RESET} - unexpected match for: $pattern"
          grep -nE -- "$pattern" "$out" | sed 's/^/     /'
          file_failed=1
        fi
        ;;
    esac
  done < <(sed -E -n 's#^[[:space:]]*//[[:space:]]*(EXPECT_NOT|EXPECT)[[:space:]]+#\1 #p' "$src")

  # a test file without a single assertion passes vacuously, which is worse than no test at all
  if [[ $assertions -eq 0 ]]; then
    echo -e "${RED}❌ $name${RESET} - no EXPECT / EXPECT_NOT comments found"
    file_failed=1
  fi

  if [[ $file_failed -eq 0 ]]; then
    echo -e "${GREEN}✅ $name${RESET}"
    ((passed++))
  else
    echo -e "     ${DIM}output: $out${RESET}"
    ((failed++))
  fi
done

echo "-------------------------------------"
echo -e "🧵 Thread safety: ${GREEN}✅ $passed${RESET}   ${RED}❌ $failed${RESET}"
[[ $failed -eq 0 ]]
