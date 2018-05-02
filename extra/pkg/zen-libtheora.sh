
function libtheora_build {
  host=$1

  lazy_download "libtheora.tar.bz2" "http://downloads.xiph.org/releases/theora/libtheora-1.1.1.tar.bz2"
  lazy_extract "libtheora.tar.bz2"
  mkgit "libtheora"

  mkdir -p libtheora/build/$host
  pushDir libtheora/build/$host

  ../../configure \
    --build=$BUILD \
    --host=$host \
    --prefix=$PREFIX/$host \
    --enable-shared \
    --disable-static \
    --disable-examples
  $MAKE || true
  $sed -i 's/\(1q \\$export_symbols\)/\1|tr -d \\\\\\\"\\r\\\\\\\"/' libtool
  $MAKE
  $MAKE install

  popDir
}

function libtheora_get_deps {
  echo "libogg"
}


