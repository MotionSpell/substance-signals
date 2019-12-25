
function libepoxy_build {
  host=$1

  lazy_download "libepoxy.tar.xz" "https://github.com/anholt/libepoxy/releases/download/1.5.4/libepoxy-1.5.4.tar.xz"
  lazy_extract "libepoxy.tar.xz"
  mkgit "libepoxy"

  # hack for native build, otherwise egl.pc isn't found
  PKG_CONFIG_PATH=/usr/lib/$host/pkgconfig:/usr/share/pkgconfig:/usr/lib/pkgconfig \
  rm -rf libepoxy/bin/$host
  mkdir -p libepoxy/bin/$host
  pushDir libepoxy/bin/$host
  meson ../.. . --prefix=$PREFIX
  ninja
  ninja install
  popDir
}

function libepoxy_get_deps {
  local a=0
}



