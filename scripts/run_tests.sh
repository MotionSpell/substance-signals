#!/usr/bin/env bash

EXTRA=${EXTRA-$PWD/sysroot}

export PATH=$PATH:$EXTRA/bin:/mingw64/bin
export LD_LIBRARY_PATH=$EXTRA/lib${LD_LIBRARY_PATH:+:}${LD_LIBRARY_PATH:-}
export DYLD_LIBRARY_PATH=$EXTRA/lib${DYLD_LIBRARY_PATH:+:}${DYLD_LIBRARY_PATH:-}

make run

# blind-run the apps so they appear in coverage reports
$BIN/player.exe                       1>/dev/null 2>/dev/null || true
$BIN/src/apps/dashcastx/dashcastx.exe 1>/dev/null 2>/dev/null || true
$BIN/bin/src/apps/mp42tsx/mp42tsx.exe 1>/dev/null 2>/dev/null || true
