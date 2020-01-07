function cppredis_build {
  host=$1

  lazy_git_clone https://github.com/cpp-redis/cpp_redis cppredis "d1fc9c7cc61111958a30a9e366a7d6acea1306c4"
  sed -i "s/WINSOCK_API_LINKAGE/\/\/WINSOCK_API_LINKAGE/" cppredis/tacopie/sources/network/common/tcp_socket.cpp
  sed -i "s/WINSOCK_API_LINKAGE/\/\/WINSOCK_API_LINKAGE/" cppredis/tacopie/sources/network/windows/windows_tcp_socket.cpp

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
