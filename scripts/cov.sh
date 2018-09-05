#!/usr/bin/env bash
# Helper script to generate a coverage report for the full test suite.
# Usage: ./cov.sh
# The result is at: bin-cov/html/index.html
set -euo pipefail
rm -rf bin-cov
readonly scriptDir=$(dirname $(readlink -f $0))

# Build instrumented version
readonly BIN=bin-cov
trap "rm -rf $BIN" EXIT # free disk space

export BIN
export CFLAGS=--coverage
export LDFLAGS=--coverage
make -j`nproc`

$scriptDir/run_tests.sh

# Generate coverage report
find $BIN -path "*/unittests/*.gcda" -delete
lcov --capture -d $BIN -o $BIN/profile-full.txt
lcov --remove $BIN/profile-full.txt '/usr/include/*' '/usr/lib/*' -o $BIN/profile.txt
genhtml -o cov-html $BIN/profile.txt

echo "Coverage report is available in cov-html/index.html"
