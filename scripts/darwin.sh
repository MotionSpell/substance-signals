#!/bin/sh
set -eu

params=()
for p in "$@" ; do
  case $p in
    -Wl,--no-undefined)
        params+=("-undefined")
        params+=("error")
      ;;
    *)
      params+=("$p")
      ;;
  esac
done

ccache "/usr/bin/g++" "${params[@]}"
