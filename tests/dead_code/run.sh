#!/usr/bin/env bash

# Dead code elimination: which assignments the pass may drop, and which it may not.
#
# Usage: tests/dead_code/run.sh [path/to/cksp]

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$DIR/../expect_suite.sh" "$DIR" "Dead code elimination" "$@"
