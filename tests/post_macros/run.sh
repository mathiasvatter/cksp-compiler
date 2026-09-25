#!/usr/bin/env bash

# <iterate_post_macro> and <literate_post_macro>: expansion after every macro is expanded.
#
# Usage: tests/post_macros/run.sh [path/to/cksp]

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$DIR/../expect_suite.sh" "$DIR" "Post macros" "$@"
