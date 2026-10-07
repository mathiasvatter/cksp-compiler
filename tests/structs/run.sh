#!/usr/bin/env bash

# Structs: declaration, construction and their diagnostics.
#
# Usage: tests/structs/run.sh [path/to/cksp]

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$DIR/../expect_suite.sh" "$DIR" "Structs" "$@"
