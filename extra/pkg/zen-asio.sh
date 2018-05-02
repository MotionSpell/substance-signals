

function asio_build {
	lazy_git_clone https://github.com/chriskohlhoff/asio asio b44805ff00

	mkdir -p $PREFIX/include/asio
	cp -r asio/asio/include/* $PREFIX/include/asio/
}

function asio_get_deps {
  local a=0
}

