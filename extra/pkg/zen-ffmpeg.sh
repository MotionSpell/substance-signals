
function ffmpeg_build {
  host=$1
  pushDir $WORK/src

  lazy_git_clone git://source.ffmpeg.org/ffmpeg.git ffmpeg 4588063f3ecd9

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

  LDFLAGS="-L$PREFIX/$host/lib -lz" \
  ../../configure \
      --prefix=$PREFIX/$host \
      --enable-pthreads \
      --disable-w32threads \
      --disable-debug \
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
      --disable-gnutls \
      --disable-openssl \
      --disable-iconv \
      --disable-bzlib \
      --enable-avresample \
      --pkg-config=pkg-config \
      --target-os=$os \
      --arch=$ARCH \
      --cross-prefix=$host-
  $MAKE
  $MAKE install
  popDir

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

