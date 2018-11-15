
function zlib_build {
  lazy_download "zlib.tar.gz" "http://zlib.net/fossils/zlib-1.2.11.tar.gz"
  lazy_extract "zlib.tar.gz"
  mkgit "zlib"

  local options=()

  # prevent CMAKE from adding -rdynamic,
  # which mingw doesn't support
  case $host in
    *mingw*)
      options+=(-DCMAKE_SYSTEM_NAME=Windows) 
      ;;
  esac

  rm -rf zlib/bin/$host
  mkdir -p zlib/bin/$host
  pushDir zlib/bin/$host
  cmake \
    -DCMAKE_INSTALL_PREFIX=$PREFIX    \
    -DCMAKE_C_COMPILER=$host-gcc      \
    -DCMAKE_CXX_COMPILER=$host-g++    \
    -DCMAKE_RC_COMPILER=$host-windres \
    ${options[@]} \
    ../..
  $MAKE
  $MAKE install
  popDir
}

function zlib_get_deps {
  local a=0
}

