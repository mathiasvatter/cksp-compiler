#!/usr/bin/env bash

# Macro and define parameters that stand for a whole name followed by a parameter list.
#
# Usage: tests/macro_names/run.sh [path/to/cksp]

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$DIR/../expect_suite.sh" "$DIR" "Macro names" "$@"
