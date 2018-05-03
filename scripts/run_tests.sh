#!/usr/bin/env bash

EXTRA=${EXTRA:$PWD/sysroot}

PATH=$PATH:$PWD/extra/bin:/mingw64/bin \
LD_LIBRARY_PATH=$EXTRA/lib${LD_LIBRARY_PATH:+:}${LD_LIBRARY_PATH:-} \
DYLD_LIBRARY_PATH=$EXTRA/lib${DYLD_LIBRARY_PATH:+:}${DYLD_LIBRARY_PATH:-} \
make run
