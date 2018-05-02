
function openssl_build {
  pushDir $WORK/src

  lazy_download "openssl.tar.gz" https://www.openssl.org/source/openssl-1.1.0g.tar.gz
  lazy_extract "openssl.tar.gz"

  mkdir -p openssl/bin/$host
  pushDir openssl/bin/$host
  pwd
  $WORK/src/openssl/config \
    --prefix=$PREFIX
  $MAKE depend
  $MAKE
  $MAKE install_dev install_runtime
  popDir

  popDir
}

function openssl_get_deps {
  local a=0
}


