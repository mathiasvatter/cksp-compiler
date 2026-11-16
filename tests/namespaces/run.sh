#!/usr/bin/env bash

# Namespaces: which declaration a name inside a namespace refers to.
#
# Usage: tests/namespaces/run.sh [path/to/cksp]

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$DIR/../expect_suite.sh" "$DIR" "Namespaces" "$@"
