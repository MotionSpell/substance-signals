#!/bin/bash
set -eu

if [ $(uname -s) == "Darwin" ]; then
  CORES=$(sysctl -n hw.logicalcpu)
else
  CORES=$(nproc)
fi

if which astyle >/dev/null ; then
  astyle -q -r \
    --lineend=linux \
    --preserve-date \
    --suffix=none \
    --indent=tab \
    --indent-after-parens \
    --indent-classes --indent-col1-comments --style=attach --keep-one-line-statements \
    '*.hpp' '*.cpp'
fi

make -j$CORES

readonly t1=$(date +%s)
scripts/run_tests.sh
readonly t2=$(date +%s)
echo "$((t2-t1))s"
