#!/usr/bin/env bash
set -euo pipefail

if ! which astyle >/dev/null ; then
  echo "astyle not found, skipping reformatting ..." >&2
  exit 0
fi

function reformat_one_file
{
  local name="$1"

  cat "$f" | astyle -q \
    --lineend=linux \
    --preserve-date \
    --suffix=none \
    --indent=tab \
    --indent-after-parens \
    --indent-classes \
    --indent-col1-comments \
    --style=attach \
    --keep-one-line-statements > "$f.new.cpp"

  if diff -u "$f" "$f.new.cpp" ; then
    rm "$f.new.cpp"
  else
    mv "$f.new.cpp" "$f"
  fi
}

find -name "*.hpp" -or -name "*.cpp" | while read f ; do
  reformat_one_file "$f"
done

