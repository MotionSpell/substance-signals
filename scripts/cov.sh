#!/usr/bin/env bash
# Helper script to generate a coverage report for the full test suite.
# Usage: ./cov.sh
# The result is at: bin-cov/html/index.html
set -euo pipefail
rm -rf bin-cov

# Build instrumented version
export BIN=bin-cov
export CFLAGS=--coverage
export LDFLAGS=--coverage
make -j`nproc`

# run test suite
LD_LIBRARY_PATH=$PWD/extra/lib${LD_LIBRARY_PATH:+:}${LD_LIBRARY_PATH:-} \
DYLD_LIBRARY_PATH=$PWD/extra/lib${DYLD_LIBRARY_PATH:+:}${DYLD_LIBRARY_PATH:-} \
make run

# Generate coverage report
lcov --capture -d bin-cov -o bin-cov/profile.txt 
genhtml -o bin-cov/html bin-cov/profile.txt  
echo "Coverage report is available in bin-cov/html/index.html"
