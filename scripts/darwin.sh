#!/bin/sh
set -eu

params=()
for p in "$@" ; do
  case $p in
    -Wl,--no-undefined)
        params+=("-undefined")
        params+=("error")
      ;;
    -static-libstdc++)
      # avoid warning from clang, which ignores this option
      ;;
    *)
      params+=("$p")
      ;;
  esac
done

ccache "/usr/bin/g++" "${params[@]}"
