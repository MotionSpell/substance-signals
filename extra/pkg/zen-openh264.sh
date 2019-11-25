function openh264_build {
  host=$1
  pushDir $WORK/src

  local ARCH=$(get_arch $host)
  local OS=$(get_os $host)

  case $host in
    *mingw*)
      OS=mingw_nt
      ;;
  esac

  lazy_download "openh264.tar.gz" "https://github.com/cisco/openh264/archive/v2.0.0.tar.gz"
  lazy_extract "openh264.tar.gz"
  mkgit "openh264"

  pushDir openh264

  # these are hardcoded in the Makefile
  $sed -i "s@^PREFIX=.*@PREFIX=$PREFIX@" Makefile
  $sed -i "s@^ARCH=.*@ARCH=$ARCH@" Makefile
  $sed -i "s@^OS=.*@OS=$OS@" Makefile

  AR=$host-ar CC=$host-gcc CXX=$host-g++ $MAKE
  AR=$host-ar CC=$host-gcc CXX=$host-g++ $MAKE install

  popDir
  popDir
}

function openh264_get_deps {
  local a=0
}
