#!/usr/bin/env bash

# How a function body reaches its call site: the expression-function decision and the lowering
# that follows from it.
#
# Usage: tests/function_inlining/run.sh [path/to/cksp]

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$DIR/../expect_suite.sh" "$DIR" "Function inlining" "$@"
