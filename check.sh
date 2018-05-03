#!/bin/bash
set -eu

if [ $(uname -s) == "Darwin" ]; then
  CORES=$(sysctl -n hw.logicalcpu)
  SED=gsed
else
  CORES=$(nproc)
  SED=sed
  find src -name "*.cpp" -or -name "*.hpp" | xargs $SED -i -e 's/\r//'
fi

mkdir -p bin

make -j$CORES

scripts/run_tests.sh
