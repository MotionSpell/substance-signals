
function libmad_get_deps {
  local a=0
}

function libmad_build {
  local host=$1
  pushDir $WORK/src

  lazy_download "libmad.tar.gz" "http://sourceforge.net/projects/mad/files/libmad/0.15.1b/libmad-0.15.1b.tar.gz"
  lazy_extract "libmad.tar.gz"

  if [ $(uname -s) == "Darwin" ]; then
    $sed -i "s/-fforce-mem//" libmad/configure
    $sed -i "s/-fthread-jumps//" libmad/configure
    $sed -i "s/-fcse-follow-jumps//" libmad/configure
    $sed -i "s/-fcse-skip-blocks//" libmad/configure
    $sed -i "s/-fregmove//" libmad/configure
    $sed -i "s/-march=i486//" libmad/configure
  else
    $sed -i "s/-fforce-mem//" libmad/configure
  fi

  autoconf_build $host "libmad"

  popDir
}


