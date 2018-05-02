
function x264_build {
  local host=$1
  local crossPrefix=$(get_cross_prefix $BUILD $host)

  lazy_git_clone "git://git.videolan.org/x264.git" x264 40bb56814e56ed342040bdbf30258aab39ee9e89

  case $host in
    *darwin*)
      RANLIB="" x264_do_build
      ;;
    *mingw*)
      x264_do_build --enable-win32thread
      ;;
    *)
      x264_do_build
      ;;
  esac
}

function x264_get_deps {
  echo "libpthread"
}

function x264_do_build {
  autoconf_build $host x264 \
    --enable-static \
    --enable-pic \
    --disable-gpl \
    --disable-cli \
    --enable-strip \
    --disable-avs \
    --disable-swscale \
    --disable-lavf \
    --disable-ffms \
    --disable-gpac \
    --disable-opencl \
    --cross-prefix=$crossPrefix \
    "$@"
}
