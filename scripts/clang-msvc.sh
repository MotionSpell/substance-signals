#!/bin/sh
set -eu

params=()
for p in "$@" ; do
  case $p in
    -fPIC)
      ;;
    -lpthread)
      ;;
    -Wl,-rpath*)
      ;;
    -g*)
	params+=("$p")
        params+=("-gcodeview")
	;;
    -O*)
      debug=${DEBUG-""}
      if [ "debug" != 1 ] ; then
        params+=("$p")
      fi
	;;
    *)
      params+=("$p")
      ;;
  esac
done

params+=("-D_CRT_SECURE_NO_WARNINGS")
params+=("-D_CRT_SECURE_NO_DEPRECATE")
params+=("-Xclang")
params+=("-flto-visibility-public-std")
ccache "/c/Program Files/LLVM/bin/clang++" "${params[@]}"

