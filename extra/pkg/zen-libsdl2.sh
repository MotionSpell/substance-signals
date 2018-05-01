
function libsdl2_build {
  local host=$1
  pushDir $WORK/src

  lazy_download "libsdl2.tar.gz" "https://www.libsdl.org/release/SDL2-2.0.6.tar.gz"
  lazy_extract "libsdl2.tar.gz"
  mkgit "libsdl2"

  autoconf_build $host "libsdl2"

  popDir
}

function libsdl2_get_deps {
  local a=0
}

