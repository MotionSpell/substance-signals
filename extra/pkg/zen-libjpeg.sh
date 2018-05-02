
function libjpeg_get_deps {
  local a=0
}

function libjpeg_build {
  local host=$1

  lazy_download "libjpeg-$host.tar.gz" "http://www.ijg.org/files/jpegsrc.v9a.tar.gz"

  lazy_extract "libjpeg-$host.tar.gz"

  autoconf_build $host "libjpeg-$host" \
    --enable-dependency-tracking
}


