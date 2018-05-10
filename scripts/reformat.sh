#!/usr/bin/env bash
set -euo pipefail

function has_astyle
{
  if ! which astyle >/dev/null ; then
    return 1
  fi

  if [ ! "$(astyle --version)" = "Artistic Style Version 3.1" ] ; then
    return 1
  fi

  return 0
}

if ! has_astyle ; then
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
    --keep-one-line-statements > "$f.new"

  if diff -u "$f" "$f.new" ; then
    rm "$f.new"
  else
    mv "$f.new" "$f"
  fi
}

find -name "*.hpp" -or -name "*.cpp" | while read f ; do
  reformat_one_file "$f"
done

