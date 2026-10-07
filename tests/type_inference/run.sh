#!/usr/bin/env bash

# Type inference: declarations and generic functions whose types are only known after
# monomorphization.
#
# Usage: tests/type_inference/run.sh [path/to/cksp]

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$DIR/../expect_suite.sh" "$DIR" "Type inference" "$@"
