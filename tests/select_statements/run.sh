#!/usr/bin/env bash

# Select parsing and fallback branch compatibility.
#
# Usage: tests/select_statements/run.sh [path/to/cksp]

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$DIR/../expect_suite.sh" "$DIR" "Select statements" "$@"
