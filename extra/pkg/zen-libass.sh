
function libass_build {
  host=$1
  pushDir $WORK/src

  lazy_download "libass.tar.xz" "https://github.com/libass/libass/releases/download/0.13.2/libass-0.13.2.tar.xz"
  lazy_extract "libass.tar.xz"
  mkgit "libass"

  autoconf_build $host "libass" \
    --enable-shared \
    --disable-static

  popDir
}

function libass_get_deps {
  echo freetype2
  echo fribidi
  echo fontconfig
}


