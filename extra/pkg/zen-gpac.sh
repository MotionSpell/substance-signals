
function gpac_build {
  host=$1

  lazy_git_clone https://github.com/gpac/gpac.git gpac ef8b230

  local OS=$(get_os $host)
  local crossPrefix=$(get_cross_prefix $BUILD $host)

  local os=$OS
  case $OS in
    darwin*)
      os="Darwin"
      ;;
  esac

  mkdir -p gpac/build/$host
  pushDir gpac/build/$host

  ../../configure \
    --target-os=$os \
    --cross-prefix="$crossPrefix" \
    --extra-cflags="-I$PREFIX/include -w -fPIC" \
    --extra-ldflags="-L$PREFIX/lib -Wl,-rpath-link=$PREFIX/lib" \
    --sdl-cfg=":$PREFIX/bin" \
    --disable-jack \
    --disable-ssl \
    --use-png=no \
    --disable-player \
    --prefix=$PREFIX

  $MAKE lib
  $MAKE install # this is what causes gpac.pc to be copied to lib/pkg-config
  $MAKE install-lib

  popDir
}

function gpac_get_deps {
  echo libpthread
  echo ffmpeg
  echo freetype2
  echo libogg
  echo libsdl2
  echo libogg
  echo zlib
}


