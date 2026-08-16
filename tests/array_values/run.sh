#!/usr/bin/env bash

# Arrays as values: as arguments of the array commands and as return values.
#
# Usage: tests/array_values/run.sh [path/to/cksp]

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$DIR/../expect_suite.sh" "$DIR" "🧮 Array values" "$@"
