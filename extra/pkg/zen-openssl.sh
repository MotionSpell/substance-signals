
function openssl_build {
  lazy_download "libressl.tar.gz" https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/libressl-2.8.2.tar.gz
  lazy_extract "libressl.tar.gz"

  autoconf_build $host "libressl"
}

function openssl_get_deps {
  local a=0
}
