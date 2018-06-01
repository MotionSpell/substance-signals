#!/usr/bin/env bash

readonly tmpDir=/tmp/signals-test-$$
trap "rm -rf $tmpDir" EXIT
mkdir -p $tmpDir

EXTRA=${EXTRA-$PWD/sysroot}

# required for MSYS
export PATH=$PATH:$EXTRA/bin:/mingw64/bin

# required for GNU/Linux
export LD_LIBRARY_PATH=$EXTRA/lib${LD_LIBRARY_PATH:+:}${LD_LIBRARY_PATH:-}

# required for ?
export DYLD_LIBRARY_PATH=$EXTRA/lib${DYLD_LIBRARY_PATH:+:}${DYLD_LIBRARY_PATH:-}

function main
{
  run_test unittests
  run_test dashcast_crashtest

  # blind-run the apps so they appear in coverage reports
  run_test player_blindtest
  run_test mp42tsx_blindtest
  echo "OK"
}

function run_test
{
  local name="$1"
  echo "* $name"
  "$name"
}

function dashcast_crashtest
{
  # dashcastx simple crash test
  $BIN/src/apps/dashcastx/dashcastx.exe \
    -w $tmpDir/dashcastx \
    $PWD/src/tests/data/h264.ts 1>/dev/null 2>/dev/null
}

function player_blindtest
{
  $BIN/player.exe 1>/dev/null 2>/dev/null || true
}

function mp42tsx_blindtest
{
  $BIN/bin/src/apps/mp42tsx/mp42tsx.exe 1>/dev/null 2>/dev/null || true
}

function unittests
{
  make run
}

main "$@"

