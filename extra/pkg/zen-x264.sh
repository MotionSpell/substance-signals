
function x264_build {
  local host=$1
  local crossPrefix=$(get_cross_prefix $BUILD $host)

  pushDir $WORK/src
  lazy_git_clone "git://git.videolan.org/x264.git" x264 40bb56814e56ed342040bdbf30258aab39ee9e89

  local build="autoconf_build $host x264 \
    --enable-static \
    --enable-pic \
    --disable-gpl \
    --disable-cli \
    $THREADING \
    --enable-strip \
    --disable-avs \
    --disable-swscale \
    --disable-lavf \
    --disable-ffms \
    --disable-gpac \
    --disable-opencl \
    --cross-prefix=$crossPrefix"
  case $host in
    *darwin*)
      RANLIB="" $build
      ;;
    *mingw*)
      THREADING="--enable-win32thread" $build
      ;;
    *)
      $build
      ;;
  esac

  popDir
}

function x264_get_deps {
  echo "libpthread"
}


