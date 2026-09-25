#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$DIR/../.." && pwd)"
EXEC="${1:-$ROOT/cmake-build-debug/cksp}"
bash "$ROOT/tests/expect_suite.sh" "$DIR" "Property accessors" "$EXEC"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
"$EXEC" -o "$TMP/returns.txt" "$DIR/read_write.ksp" > "$TMP/returns.log" 2>&1
"$EXEC" -o "$TMP/indices.txt" "$DIR/arrays_chains.ksp" > "$TMP/indices.log" 2>&1
python3 - "$TMP" <<'PY'
from pathlib import Path
import re
import sys
root = Path(sys.argv[1])
returns = (root / 'returns.txt').read_text()
read = re.search(r'\$tmp\w* := %Value____number\[\$local_value\w*\]', returns)
clear = re.search(r'%Value____number\[[^\]]+\] := 0', returns)
assert read and clear and read.start() < clear.start(), 'Getter must run before local object cleanup'
indices = (root / 'indices.txt').read_text()
assert indices.count('message(999)') == 1, 'An index expression must be evaluated exactly once'
print('Getter lifetime and single index evaluation: OK')
PY
