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

/usr/bin/env time -f "%e" scripts/run_tests.sh
