
function libogg_build {
  host=$1

  lazy_download "libogg.tar.gz" "http://downloads.xiph.org/releases/ogg/libogg-1.3.3.tar.gz"
  lazy_extract "libogg.tar.gz"
  mkgit "libogg"

  autoconf_build $host "libogg"
}

function libogg_get_deps {
  echo ""
}


