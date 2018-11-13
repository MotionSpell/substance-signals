
function libsdl2_build {
  local host=$1

  lazy_download "libsdl2.tar.gz" "https://www.libsdl.org/release/SDL2-2.0.9.tar.gz"
  lazy_extract "libsdl2.tar.gz"
  mkgit "libsdl2"

  autoconf_build $host "libsdl2"

  # fix SDL2 leaking flags to the user build system
  # this one in particular is incompatible with the usage of <thread> and pthreads
  # (see: gcc #52590)
  $sed -i "s/-static-libgcc//" $PREFIX/lib/pkgconfig/sdl2.pc
}

function libsdl2_get_deps {
  local a=0
}

