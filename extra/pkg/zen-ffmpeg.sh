
function ffmpeg_build {
  host=$1

  lazy_git_clone git://source.ffmpeg.org/ffmpeg.git ffmpeg fe84f70819d6f5aab3c4823290e0d32b99d6de78

  local ARCH=$(get_arch $host)
  local OS=$(get_os $host)

  local os=$OS
  case $OS in
    darwin*)
      os="darwin"
      ;;
  esac

  # remove stupid dependency
  $sed -i "s/jack_jack_h pthreads/jack_jack_h/" ffmpeg/configure

  mkdir -p ffmpeg/build/$host
  pushDir ffmpeg/build/$host

  LDFLAGS="-L$PREFIX/lib -lz" \
  ../../configure \
      --prefix=$PREFIX \
      --enable-pthreads \
      --disable-w32threads \
      --disable-debug \
      --disable-doc \
      --disable-static \
      --enable-shared \
      --enable-libass \
      --enable-fontconfig \
      --enable-librtmp \
      --enable-gpl \
      --enable-libx264 \
      --enable-zlib \
      --disable-gnutls \
      --disable-openssl \
      --disable-iconv \
      --disable-bzlib \
      --enable-avresample \
      --disable-decoder=mp3float \
      --pkg-config=pkg-config \
      --target-os=$os \
      --arch=$ARCH \
      --cross-prefix=$host-
  $MAKE
  $MAKE install
  popDir
}

function ffmpeg_get_deps {
  echo libass
  echo fontconfig
  echo librtmp
  echo libpthread
  echo x264
  echo zlib
}

