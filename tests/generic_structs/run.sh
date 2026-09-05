#!/usr/bin/env bash

# Parsing and diagnostics for generic struct type-parameter lists.
#
# Usage: tests/generic_structs/run.sh [path/to/cksp]

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$DIR/../expect_suite.sh" "$DIR" "Generic structs" "$@"
