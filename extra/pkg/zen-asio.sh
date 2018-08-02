
function asio_build {

	lazy_download "asio.tar.gz" https://github.com/chriskohlhoff/asio/archive/asio-1-12-0.tar.gz
	lazy_extract "asio.tar.gz"
	mkgit asio

	mkdir -p $PREFIX/include/asio
	cp -r asio/asio/include/* $PREFIX/include/asio/
}

function asio_get_deps {
  local a=0
}

