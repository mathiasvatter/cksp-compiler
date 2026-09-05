#!/usr/bin/env bash

# Runs a suite of .ksp files that assert on the KSP their compilation produces.
#
# Each file carries its expectations as comments:
#
#   // EXPECT       <extended regex>   -> must match somewhere in the generated KSP
#   // EXPECT_NOT   <extended regex>   -> must not match anywhere in the generated KSP
#   // EXPECT_ERROR <extended regex>   -> compilation must fail and report this
#   // EXPECT_WARNING <extended regex> -> compilation must succeed and report this warning
#   // EXPECT_WARNING_NOT <regex>      -> compilation must not report this warning
#
# A file carrying EXPECT_ERROR is expected not to compile; every other file is. Generated
# identifiers get a gensym suffix, so the patterns match on the shape of a line rather than on a
# literal name. The callback depth is pinned so <1000> stays the expanded size regardless of the
# default.
#
# Usage: tests/expect_suite.sh <suite-dir> <label> [path/to/cksp]
#
# The suites that use it are the run.sh next to their own files, e.g. tests/array_values/run.sh.

set -u

DIR="$(cd "${1:?usage: expect_suite.sh <suite-dir> <label> [cksp]}" && pwd)"
LABEL="${2:?usage: expect_suite.sh <suite-dir> <label> [cksp]}"
BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EXEC="${3:-$BASE_DIR/cmake-build-release/cksp}"
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
  log="$TMP/$name.log"

  assertions_of() {
    sed -E -n "s#^[[:space:]]*//[[:space:]]*($1)[[:space:]]+#\1 #p" "$src"
  }

  # the diagnostic is wrapped in colour codes and split over several lines, so the log is
  # flattened to one plain line before the error patterns are matched against it
  expects_error=false
  [[ -n "$(assertions_of 'EXPECT_ERROR')" ]] && expects_error=true
  expects_warning=false
  [[ -n "$(assertions_of 'EXPECT_WARNING|EXPECT_WARNING_NOT')" ]] && expects_warning=true

  file_failed=0
  compiled=true
  "$EXEC" -o "$out" -s "$DEPTH" "$src" >"$log" 2>&1 || compiled=false

  if [[ "$expects_error" == true ]]; then
    if [[ "$compiled" == true ]]; then
      echo -e "${RED}❌ $name${RESET} - expected the compilation to fail, but it succeeded"
      file_failed=1
    fi
  elif [[ "$compiled" == false ]]; then
    echo -e "${RED}❌ $name${RESET} - compile failed"
    sed 's/^/     /' "$log" | tail -5
    ((failed++))
    continue
  fi
  if [[ "$expects_error" == true || "$expects_warning" == true ]]; then
    # the report ends in a github issue url that repeats the message percent encoded, which would
    # match patterns the diagnostic itself never printed
    sed -e 's/\x1b\[[0-9;]*m//g' "$log" | sed -e 's#https://github.com/[^ ]*##' | tr '\n' ' ' > "$log.flat"
  fi

  assertions=0
  while IFS= read -r assertion; do
    ((assertions++))
    [[ -z "$assertion" ]] && continue
    kind="${assertion%% *}"
    pattern="${assertion#"$kind" }"

    case "$kind" in
      EXPECT)
        if ! grep -qE -- "$pattern" "$out"; then
          echo -e "${RED}❌ $name${RESET} - expected a match for: $pattern"
          file_failed=1
        fi
        ;;
      EXPECT_NOT)
        if grep -qE -- "$pattern" "$out"; then
          echo -e "${RED}❌ $name${RESET} - unexpected match for: $pattern"
          grep -nE -- "$pattern" "$out" | sed 's/^/     /'
          file_failed=1
        fi
        ;;
      EXPECT_ERROR)
        if ! grep -qE -- "$pattern" "$log.flat"; then
          echo -e "${RED}❌ $name${RESET} - expected the compiler to report: $pattern"
          file_failed=1
        fi
        ;;
      EXPECT_WARNING)
        if ! grep -qE -- "$pattern" "$log.flat"; then
          echo -e "${RED}❌ $name${RESET} - expected the compiler to warn: $pattern"
          file_failed=1
        fi
        ;;
      EXPECT_WARNING_NOT)
        if grep -qE -- "$pattern" "$log.flat"; then
          echo -e "${RED}❌ $name${RESET} - unexpected warning match for: $pattern"
          file_failed=1
        fi
        ;;
    esac
  done < <(assertions_of 'EXPECT_WARNING_NOT|EXPECT_WARNING|EXPECT_NOT|EXPECT_ERROR|EXPECT')

  # a test file without a single assertion passes vacuously, which is worse than no test at all
  if [[ $assertions -eq 0 ]]; then
    echo -e "${RED}❌ $name${RESET} - no EXPECT assertions found"
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
echo -e "$LABEL: ${GREEN}✅ $passed${RESET}   ${RED}❌ $failed${RESET}"
[[ $failed -eq 0 ]]
