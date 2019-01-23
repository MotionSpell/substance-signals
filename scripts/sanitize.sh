#!/usr/bin/env bash
# Helper script to run an instrumented version
# of the unit tests.
# Usage: ./scripts/sanitize.sh
set -euo pipefail
readonly scriptDir=$(dirname $(readlink -f $0))

# Build instrumented version

# the 'thread' sanitizer doesn't play well with the others
# (change the below condition to thread-sanitize your code)
if false ; then
  BIN=bin-tsan
  CFLAGS=-fsanitize=thread
  LDFLAGS=-fsanitize=thread
else
  BIN=bin-asan
  CFLAGS="-fsanitize=address,undefined -fno-sanitize=vptr"
  LDFLAGS=-fsanitize=address,undefined
fi

CFLAGS="$CFLAGS -g3"
LDFLAGS="$LDFLAGS -g"

export BIN CFLAGS LDFLAGS

make -j`nproc`

export TSAN_OPTIONS=halt_on_error=1
export ASAN_OPTIONS=halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1
$scriptDir/run_tests.sh

