function ffnvenc_build {
  lazy_download "nv-codec-headers.tar.gz" "https://github.com/FFmpeg/nv-codec-headers/releases/download/n11.0.10.0/nv-codec-headers-11.0.10.0.tar.gz"
  lazy_extract "nv-codec-headers.tar.gz"
  mkgit "nv-codec-headers"
  pushDir nv-codec-headers
  sed -i "s|/usr/local|$PREFIX|g" Makefile
  make
  make install
  popDir
}

function ffnvenc_get_deps {
  local a=0
}