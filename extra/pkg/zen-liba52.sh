
function liba52_build {
  host=$1

  lazy_download "liba52.tar.xz" "http://liba52.sourceforge.net/files/a52dec-0.7.4.tar.gz"
  lazy_extract "liba52.tar.xz"

  CFLAGS="-w -fPIC -std=gnu89" \
  autoconf_build $host "liba52" \
    --enable-shared \
    --disable-static

}

function liba52_get_deps {
  local a=0
}


