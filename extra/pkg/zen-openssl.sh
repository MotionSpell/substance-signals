
function openssl_build {
  lazy_download "openssl.tar.gz" https://www.openssl.org/source/openssl-1.1.0h.tar.gz
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
    *-mingw32)
      openssl_os="mingw"
      ;;
    *)
      echo "ERROR: platform not implemented: $host" >&2
      exit 1
  esac

  $WORK/src/openssl/Configure \
    $openssl_os \
    shared \
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
