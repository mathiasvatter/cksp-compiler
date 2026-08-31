#!/usr/bin/env bash

# Type casts: parsing, type inference, generic substitution and lowering.
#
# Usage: tests/casts/run.sh [path/to/cksp]

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$DIR/../expect_suite.sh" "$DIR" "Type casts" "$@"
