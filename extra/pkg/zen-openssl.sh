
function openssl_build {
  lazy_download "openssl.tar.gz" https://www.openssl.org/source/openssl-1.1.1.tar.gz
  lazy_extract "openssl.tar.gz"

  mkdir -p openssl/bin/$host
  pushDir openssl/bin/$host

  # OpenSSL uses its own OS naming scheme
  # and doesn't support standard compiler prefix
  case $host in
    x86_64-*-gnu)
      openssl_os="linux-x86_64"
      ;;
    i686-*-gnu)
      openssl_os="linux-x86"
      ;;
    x86_64-*-mingw32)
      openssl_os="mingw64"
      ;;
    i686-*-mingw32)
      openssl_os="mingw"
      ;;
    x86_64-apple-darwin*)
      openssl_os="darwin64-x86_64-cc"
      ;;
    *)
      echo "ERROR: platform not implemented: $host" >&2
      exit 1
  esac

  ../../Configure \
    $openssl_os \
    shared \
    no-unit-test \
    no-tests \
    --cross-compile-prefix="$host-" \
    --prefix=$PREFIX
  $MAKE depend
  $MAKE
  $MAKE install_dev install_runtime
  popDir
}

function openssl_get_deps {
  local a=0
}
