
function libxvidcore_get_deps {
  local a=0
}

function libxvidcore_build {
  local host=$1

  lazy_download "libxvidcore.tar.gz" "http://downloads.xvid.org/downloads/xvidcore-1.3.3.tar.gz"
  lazy_extract "libxvidcore.tar.gz"

  pushDir libxvidcore/build/generic

  cp $scriptDir/config.guess .
  ./configure \
     --host=$host \
     --prefix=$PREFIX/$host

  $MAKE
  $MAKE install


  popDir
}


