#!/usr/bin/env bash
# Helper script to generate a coverage report for the full test suite.
# Usage: ./cov.sh
# The result is at: bin-cov/html/index.html
set -euo pipefail
readonly scriptDir=$(dirname $(readlink -f $0))

# Build instrumented version
export BIN=bin-san
export CFLAGS=-fsanitize=address,undefined
export LDFLAGS=-fsanitize=address,undefined
make -j`nproc`

export ASAN_OPTIONS=halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1
$scriptDir/run_tests.sh

