#!/usr/bin/env bash

# Import resolution: aliases, the namespace an alias produces, and the diagnostics for importing
# one file twice. The files under fixtures/ are imported by the tests and are not tests themselves.
#
# Usage: tests/imports/run.sh [path/to/cksp]

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$DIR/../expect_suite.sh" "$DIR" "Imports" "$@"
