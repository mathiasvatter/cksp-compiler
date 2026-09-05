#!/usr/bin/env bash

# Constants the compiler folds have to come out as the engine would have computed them.
#
# Usage: tests/constant_folding/run.sh [path/to/cksp]

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$DIR/../expect_suite.sh" "$DIR" "🔢 Constant folding" "$@"
