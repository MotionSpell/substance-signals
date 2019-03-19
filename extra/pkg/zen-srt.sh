
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

  # Workaround: for some reason, 'libsrt.so.1.3.2' that gets installed by
  # 'make install' differs from the one from the build directory,
  # and is unable to find its dependencies (libcrypto, libssl), making the
  # ffmpeg 'configure' fail.
  if ! diff -q "srt/bin/$host/libsrt.so.1.3.2" "$PREFIX/lib/libsrt.so.1.3.2" ; then
    echo "WTF!"
  fi

  cp "srt/bin/$host/libsrt.so.1.3.2" $PREFIX/lib
}

function srt_get_deps {
  echo openssl
}

