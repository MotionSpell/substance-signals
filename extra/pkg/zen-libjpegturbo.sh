
function libjpeg-turbo_build {
  lazy_download "libjpeg-turbo.tar.gz" "https://sourceforge.net/projects/libjpeg-turbo/files/2.0.3/libjpeg-turbo-2.0.3.tar.gz/download"
  lazy_extract "libjpeg-turbo.tar.gz"


  rm -rf libjpeg-turbo/bin/$host
  mkdir -p libjpeg-turbo/bin/$host
  pushDir libjpeg-turbo/bin/$host
  cmake \
    -DOPENSSL_ROOT_DIR=$PREFIX \
    -DCURL_ROOT_DIR=$PREFIX \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS=-I$PREFIX/include \
    -DCMAKE_LD_FLAGS=-L$PREFIX/lib \
    -DCMAKE_INSTALL_PREFIX=$PREFIX \
    -DCMAKE_INSTALL_LIBDIR=lib \
    ../..
  $MAKE
  $MAKE install
  popDir
}

function libjpeg-turbo_get_deps {
  local a=0
}

