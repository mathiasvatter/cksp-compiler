#!/usr/bin/env bash

# Reference counting: retains and releases around pointer assignments, declarations and scopes.
#
# Usage: tests/ref_counting/run.sh [path/to/cksp]

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$DIR/../expect_suite.sh" "$DIR" "Reference counting" "$@"
