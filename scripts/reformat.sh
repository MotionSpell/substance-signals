#!/usr/bin/env bash
set -euo pipefail

if ! which astyle >/dev/null ; then
  echo "astyle not found, skipping reformatting ..." >&2
  exit 0
fi

astyle -q -r \
  --lineend=linux \
  --preserve-date \
  --suffix=none \
  --indent=tab \
  --indent-after-parens \
  --indent-classes --indent-col1-comments --style=attach --keep-one-line-statements \
  '*.hpp' '*.cpp'

