
function zlib_build {
  lazy_download "zlib.tar.gz" "http://zlib.net/fossils/zlib-1.2.11.tar.gz"
  lazy_extract "zlib.tar.gz"
  mkgit "zlib"

  local options=()

  options+=(-DCMAKE_INSTALL_PREFIX=$PREFIX)

  # prevent CMAKE from adding -rdynamic,
  # which mingw doesn't support
  case $host in
    *mingw*)
      options+=(-DCMAKE_SYSTEM_NAME=Windows)
      ;;
    *darwin*)
      options+=(-DCMAKE_SYSTEM_NAME=Darwin)
      options+=(-DAPPLE=1)
      ;;
  esac

  rm -rf zlib/bin/$host
  mkdir -p zlib/bin/$host
  pushDir zlib/bin/$host
  cmake \
    -DCMAKE_C_COMPILER=$host-gcc      \
    -DCMAKE_CXX_COMPILER=$host-g++    \
    -DCMAKE_RC_COMPILER=$host-windres \
    ${options[@]} \
    ../..
  $MAKE
  $MAKE install
  popDir

  # zlib's pkg-config does '-lz', but this won't work as the
  # mingw build of zlib doesn't build a 'libz.a'.
  # So make one.
  case $host in
    *mingw*)
      cp $PREFIX/lib/libzlib.dll.a $PREFIX/lib/libz.a
      ;;
  esac
}

function zlib_get_deps {
  local a=0
}

