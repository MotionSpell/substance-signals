
function libpthread_get_deps {
  local a=0
}

function libpthread_build {
  case $host in
    *mingw*)
      libpthread_build_mingw $@
      ;;
    *)
      libpthread_build_native $@
      ;;
  esac
}

function libpthread_build_mingw {
  local host=$1

  lazy_download "mingw-w64.tar.bz2" "http://sourceforge.net/projects/mingw-w64/files/mingw-w64/mingw-w64-release/mingw-w64-v3.1.0.tar.bz2/download"
  lazy_extract "mingw-w64.tar.bz2"
  pushDir mingw-w64/mingw-w64-libraries

  autoconf_build $host "winpthreads"

  rm $PREFIX/$host/include/pthread.h #remove broken header - will fallback on the pthread correct one

  popDir
}

function libpthread_build_native {
  local host=$1
  pushDir $WORK/src

  lazy_download "libpthread.tar.bz2" "http://xcb.freedesktop.org/dist/libpthread-stubs-0.1.tar.bz2"
  lazy_extract "libpthread.tar.bz2"
  autoconf_build $host "libpthread"

  popDir
}

