

function asio_build {
	lazy_git_clone https://github.com/chriskohlhoff/asio asio b44805ff00

	mkdir -p $PREFIX/$host/include/asio
	cp -r asio/asio/include/* $PREFIX/$host/include/asio/
}

function asio_get_deps {
  local a=0
}

