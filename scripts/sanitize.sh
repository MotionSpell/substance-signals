#!/usr/bin/env bash
# Helper script to run an instrumented version
# of the unit tests.
# Usage: ./scripts/sanitize.sh
set -euo pipefail
readonly scriptDir=$(dirname $(readlink -f $0))

# Build instrumented version
export BIN=bin-san

# the 'thread' sanitizer doesn't play well with the others
# (change the below condition to thread-sanitize your code)
if false ; then
  export CFLAGS=-fsanitize=thread
  export LDFLAGS=-fsanitize=thread
else
  export CFLAGS=-fsanitize=address,undefined
  export LDFLAGS=-fsanitize=address,undefined
fi
make -j`nproc`

export TSAN_OPTIONS=halt_on_error=1
export ASAN_OPTIONS=halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1
$scriptDir/run_tests.sh

