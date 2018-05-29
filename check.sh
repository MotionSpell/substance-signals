#!/bin/bash
set -eu

if [ $(uname -s) == "Darwin" ]; then
  CORES=$(sysctl -n hw.logicalcpu)
else
  CORES=$(nproc)
fi

./scripts/reformat.sh

make -j$CORES

if ! ./scripts/check_vcxproj ; then
  echo "vcxproj files are out of date" >&2
  exit 1
fi

readonly t1=$(date +%s)
scripts/run_tests.sh
readonly t2=$(date +%s)
echo "$((t2-t1))s"
