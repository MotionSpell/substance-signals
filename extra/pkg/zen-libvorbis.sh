
function libvorbis_build {
  host=$1
  pushDir $WORK/src

  lazy_download "libvorbis.tar.gz" "http://downloads.xiph.org/releases/vorbis/libvorbis-1.3.4.tar.gz"
  lazy_extract "libvorbis.tar.gz"
  mkgit "libvorbis"

  autoconf_build $host "libvorbis"

  popDir
}

function libvorbis_get_deps {
  echo "libogg"
}


