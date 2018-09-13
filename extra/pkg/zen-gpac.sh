
function gpac_build {
  host=$1

  # do not use a truncated hash here, use the full hash!
  # (collisions DO occur with truncated hashes, in practice this would
  # have the effect of stopping the whole build)
  readonly hash="b6f7409ce2ad68b6820dacb48430c25e0d0854a0"

  lazy_git_clone https://github.com/gpac/gpac.git gpac "$hash"

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
    --extra-ldflags="-L$PREFIX/lib" \
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


