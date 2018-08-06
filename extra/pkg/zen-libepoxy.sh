
function libepoxy_build {
  host=$1

  lazy_download "libepoxy.tar.xz" "https://github.com/anholt/libepoxy/releases/download/1.5.2/libepoxy-1.5.2.tar.xz"
  lazy_extract "libepoxy.tar.xz"
  mkgit "libepoxy"

  autoconf_build $host "libepoxy" \
    --enable-shared \
    --disable-static
}

function libepoxy_get_deps {
  local a=0
}



