
function openssl_build {
  lazy_download "libressl.tar.gz" https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/libressl-3.0.2.tar.gz
  lazy_extract "libressl.tar.gz"

  # Don't build manpages.
  # We don't use them, it's slow on all platforms, and it fails under MSYS2.
  sed 's/^SUBDIRS = \(.*\) man/SUBDIRS = \1/' -i libressl/Makefile.in

  # static lib needed for the TLS handshake table
  CC=$host-gcc \
  autoconf_build $host "libressl" \
    "--enable-shared" \
    # "--disable-hardening" #uncomment if your build fails
}

function openssl_get_deps {
  local a=0
}
