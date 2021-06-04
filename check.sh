#!/bin/bash
set -eu

export BIN=bin

if [ $(uname -s) == "Darwin" ]; then
  CORES=$(sysctl -n hw.logicalcpu)
else
  CORES=$(nproc)
fi

#./scripts/reformat.sh

make -j$CORES

readonly t1=$(date +%s)
scripts/run_tests.sh
readonly t2=$(date +%s)
echo "$((t2-t1))s"

lines=$(find $BIN -name "*.lines" | xargs cat | awk '{s+=$1} END {printf "%d\n", (s/1000)}')
echo "compilation mass: $lines kloc"
