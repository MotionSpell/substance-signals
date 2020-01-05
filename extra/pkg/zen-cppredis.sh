function cppredis_build {
  host=$1

  readonly hash=bbe38a7f83de943ffcc90271092d689ae02b3489 #v4.3.1
  lazy_git_clone https://github.com/Cylix/cpp_redis.git cppredis "$hash"

  rm -rf cppredis/bin/$host
  mkdir -p cppredis/bin/$host
  pushDir cppredis/bin/$host
  cmake -G "Unix Makefiles" -DCMAKE_CXX_COMPILER="$host-gcc" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS=-I$PREFIX/include \
    -DCMAKE_LD_FLAGS=-L$PREFIX/lib \
    -DCMAKE_INSTALL_PREFIX=$PREFIX \
    -DCMAKE_INSTALL_LIBDIR=lib \
    ../..
  $MAKE
  $MAKE install
  popDir
}

function cppredis_get_deps {
  local a=0
}
