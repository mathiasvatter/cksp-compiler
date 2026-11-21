#!/usr/bin/env bash

# Which locals ASTVariableReuse lets share one KSP variable, and which it must keep apart.
#
# Usage: tests/variable_reuse/run.sh [path/to/cksp]

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$DIR/../expect_suite.sh" "$DIR" "Variable reuse" "$@"
