
function expat_build {
  local host=$1

  lazy_download "expat.tar.xz" "http://sourceforge.net/projects/expat/files/expat/2.1.0/expat-2.1.0.tar.gz/download"
  lazy_extract "expat.tar.xz"
  mkgit "expat"

  CFLAGS+=" -I$PREFIX/include " \
  LDFLAGS+=" -L$PREFIX/lib " \
  autoconf_build $host "expat" \
    --enable-shared \
    --disable-static
}

function expat_get_deps {
  local a=0
}

