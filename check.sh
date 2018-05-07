#!/bin/bash
set -eu

if [ $(uname -s) == "Darwin" ]; then
  CORES=$(sysctl -n hw.logicalcpu)
else
  CORES=$(nproc)
fi

./scripts/reformat.sh

make -j$CORES

readonly t1=$(date +%s)
scripts/run_tests.sh
readonly t2=$(date +%s)
echo "$((t2-t1))s"
