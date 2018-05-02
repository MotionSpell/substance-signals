
function libpng_build {
  host=$1

  lazy_download "libpng.tar.xz" "http://prdownloads.sourceforge.net/libpng/libpng-1.2.52.tar.xz?download"
  lazy_extract "libpng.tar.xz"
  mkgit "libpng"

  LDFLAGS+=" -L$WORK/release/$host/lib" \
  CFLAGS+=" -I$WORK/release/$host/include" \
  CPPFLAGS+=" -I$WORK/release/$host/include" \
  autoconf_build $host "libpng" \
    --enable-shared \
    --disable-static
}

function libpng_get_deps {
  echo "zlib"
}


