
function srt_build {
  lazy_download "srt.tar.gz" "https://github.com/Haivision/srt/archive/v1.3.2.tar.gz"
  lazy_extract "srt.tar.gz"
  mkgit "srt"

  rm -rf srt/bin/$host
  mkdir -p srt/bin/$host
  pushDir srt/bin/$host

  local systemName=""

  case $host in
    *mingw*)
      systemName="Windows"
      ;;
    *linux*)
      systemName="Linux"
      ;;
  esac

  # ENABLE_CXX11=OFF: disable building of test apps, whose linking is broken
  # CMAKE_SYSTEM_NAME: without it, cmake doesn't realize we're cross-compiling,
  # and tries to use native-specific compiler options (e.g -rdynamic).
  cmake \
    -DCMAKE_SYSTEM_NAME=$systemName \
    -DCMAKE_C_COMPILER=$host-gcc      \
    -DCMAKE_CXX_COMPILER=$host-g++    \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$PREFIX \
    -DENABLE_CXX11=OFF \
    -DENABLE_SHARED=ON \
    -DENABLE_STATIC=OFF \
    -DENABLE_INET_PTON=ON \
    ../..
  $MAKE
  $MAKE install
  popDir

  # Workaround: for some reason, 'libsrt.so.1.3.2' that gets installed by
  # 'make install' differs from the one from the build directory,
  # and is unable to find its dependencies (libcrypto, libssl), making the
  # ffmpeg 'configure' fail.
  case $host in
    *linux*)
      if ! diff -q "srt/bin/$host/libsrt.so.1.3.2" "$PREFIX/lib/libsrt.so.1.3.2" ; then
        echo "WTF!"
      fi

      cp "srt/bin/$host/libsrt.so.1.3.2" $PREFIX/lib
      ;;
  esac
}

function srt_get_deps {
  echo openssl
}

