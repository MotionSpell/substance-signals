#!/usr/bin/env bash
# Helper script to generate a coverage report for the full test suite.
# Usage: ./cov.sh
# The result is at: bin-cov/html/index.html
set -euo pipefail
rm -rf bin-cov
readonly scriptDir=$(dirname $(readlink -f $0))

# Build instrumented version
export BIN=bin-cov
export CFLAGS=--coverage
export LDFLAGS=--coverage
make -j`nproc`

$scriptDir/run_tests.sh

# Generate coverage report
find bin-cov -path "*/unittests/*.gcda" -delete
lcov --capture -d bin-cov -o bin-cov/profile.txt
genhtml -o cov-html bin-cov/profile.txt

# free disk space
rm -rf bin-cov

echo "Coverage report is available in cov-html/index.html"
