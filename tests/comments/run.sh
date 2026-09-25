#!/usr/bin/env bash
set -eu
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
bash "$DIR/../expect_suite.sh" "$DIR" "Comments" "$@"
