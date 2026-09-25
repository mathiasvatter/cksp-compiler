#!/usr/bin/env bash
# List blocks, jagged storage, and diagnostics for incremental SublimeKSP lists.
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$DIR/../expect_suite.sh" "$DIR" "Lists" "$@"
