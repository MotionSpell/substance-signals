
function openssl_build {
  lazy_download "libressl.tar.gz" https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/libressl-2.8.2.tar.gz
  lazy_extract "libressl.tar.gz"

  # Don't build manpages.
  # We don't use them, it's slow on all platforms, and it fails under MSYS2.
  sed 's/^SUBDIRS = \(.*\) man/SUBDIRS = \1/' -i libressl/Makefile.in

  autoconf_build $host "libressl" \
    "--enable-shared" \
    "--disable-static"
}

function openssl_get_deps {
  local a=0
}
