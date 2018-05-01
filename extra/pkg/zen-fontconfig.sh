
function fontconfig_build {
  host=$1
  pushDir $WORK/src

  lazy_download "fontconfig.tar.bz2" "http://www.freedesktop.org/software/fontconfig/release/fontconfig-2.12.6.tar.bz2"
  lazy_extract "fontconfig.tar.bz2"
  mkgit "fontconfig"
  pushDir fontconfig

  autoreconf -fiv

  mkdir -p build/$host
  pushDir build/$host
  ../../configure \
    --build=$BUILD \
    --host=$host \
    --prefix=$PREFIX/$host \
    --enable-shared \
    --disable-static
  $MAKE
  $MAKE install || true #avoid error: /usr/bin/install: cannot stat
  popDir

  popDir
  popDir
}

function fontconfig_get_deps {
  echo "expat"
  echo "freetype2"
}

