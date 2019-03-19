
function srt_build {
  lazy_download "srt.tar.gz" "https://github.com/Haivision/srt/archive/v1.3.2.tar.gz"
  lazy_extract "srt.tar.gz"
  mkgit "srt"

  rm -rf srt/bin/$host
  mkdir -p srt/bin/$host
  pushDir srt/bin/$host
  cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$PREFIX \
    -DENABLE_SHARED=ON \
    -DENABLE_STATIC=OFF \
    ../..
  $MAKE
  $MAKE install
  popDir
}

function srt_get_deps {
  echo openssl
}

