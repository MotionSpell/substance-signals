
function libpng_build {
  host=$1

  lazy_download "libpng.tar.xz" "http://prdownloads.sourceforge.net/libpng/libpng-1.2.52.tar.xz?download"
  lazy_extract "libpng.tar.xz"
  mkgit "libpng"

  # workaround 'zlib not installed' (libpng's BS doesn't use pkg-config for zlib)
  LDFLAGS+=" -L$PREFIX/lib" \
  CFLAGS+=" -I$PREFIX/include" \
  CPPFLAGS+=" -I$PREFIX/include" \
  autoconf_build $host "libpng" \
    --enable-shared \
    --disable-static
}

function libpng_get_deps {
  echo "zlib"
}


