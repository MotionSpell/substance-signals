
function gpac_build {
  host=$1
  pushDir $WORK/src

  lazy_git_clone https://github.com/gpac/gpac.git gpac e0ac0849fea

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
    --extra-cflags="-I$PREFIX/$host/include -w -fPIC" \
    --extra-ldflags="-L$PREFIX/$host/lib -Wl,-rpath-link=$PREFIX/$host/lib" \
    --sdl-cfg=":$PREFIX/$host/bin" \
    --disable-jack \
    --enable-amr \
    --prefix=$PREFIX/$host

  $MAKE
  $MAKE install
  $MAKE install-lib

  popDir
  popDir
}

function gpac_get_deps {
  echo libpthread
  echo faad2
  echo ffmpeg
  echo freetype2
  echo liba52
  echo libjpeg
  echo libmad
  echo libogg
  echo libpng
  echo libsdl2
  echo libtheora
  echo libvorbis
  echo libxvidcore
  echo libogg
  echo opencore-amr
  echo zlib
}


