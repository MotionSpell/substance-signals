
function ffmpeg_build {
  host=$1

  lazy_download "ffmpeg.tar.gz" "http://ffmpeg.org/releases/ffmpeg-4.0.tar.bz2"
  lazy_extract "ffmpeg.tar.gz"
  mkgit "ffmpeg"

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
  echo librtmp
  echo libpthread
  echo x264
  echo zlib
}

