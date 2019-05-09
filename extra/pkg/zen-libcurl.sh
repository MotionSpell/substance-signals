function libcurl_build {
  lazy_download "libcurl.tar.gz" "https://curl.haxx.se/download/curl-7.62.0.tar.bz2"
  lazy_extract "libcurl.tar.gz"

  local options=()

  # Only enable what's needed, disable everything else:
  # We don't want any autodetection, as this would make depend
  # the resulting feature on the build environment!

  # options+=(--disable-rtsp)              # Disable RTSP support
  # options+=(--without-librtmp)           # disable LIBRTMP
  # options+=(--without-zlib)              # disable use of zlib
  # options+=(--disable-http)              # Disable HTTP support
  # options+=(--disable-ipv6)              # Disable IPv6 support
  options+=(--with-ssl=$PREFIX)            # enable OpenSSL

  options+=(--disable-threaded-resolver) # Disable threaded resolver
  options+=(--disable-debug)             # Disable debug build options
  options+=(--disable-warnings)          # Disable strict compiler warnings
  options+=(--disable-werror)            # Disable compiler warnings as errors
  options+=(--disable-curldebug)         # Disable curl debug memory tracking
  options+=(--disable-ares)              # Disable c-ares for DNS lookups
  options+=(--disable-rt)                # disable dependency on -lrt
  options+=(--disable-ftp)               # Disable FTP support
  options+=(--disable-file)              # Disable FILE support
  options+=(--disable-ldap)              # Disable LDAP support
  options+=(--disable-ldaps)             # Disable LDAPS support
  options+=(--disable-proxy)             # Disable proxy support
  options+=(--disable-dict)              # Disable DICT support
  options+=(--disable-telnet)            # Disable TELNET support
  options+=(--disable-tftp)              # Disable TFTP support
  options+=(--disable-pop3)              # Disable POP3 support
  options+=(--disable-imap)              # Disable IMAP support
  options+=(--disable-smb)               # Disable SMB/CIFS support
  options+=(--disable-smtp)              # Disable SMTP support
  options+=(--disable-gopher)            # Disable Gopher support
  options+=(--disable-manual)            # Disable built-in manual
  options+=(--disable-pthreads)          # Disable POSIX threads
  options+=(--disable-sspi)              # Disable SSPI
  options+=(--disable-crypto-auth)       # Disable cryptographic authentication
  options+=(--disable-ntlm-wb)           # Disable NTLM delegation to winbind's ntlm_auth
  options+=(--disable-tls-srp)           # Disable TLS-SRP authentication
  options+=(--disable-unix-sockets)      # Disable Unix domain sockets
  options+=(--disable-cookies)           # Disable cookies support
  options+=(--without-brotli)            # disable BROTLI
  options+=(--without-winssl)            # disable Windows native SSL/TLS
  options+=(--without-darwinssl)         # disable Apple OS native SSL/TLS
  options+=(--without-gnutls)            # disable GnuTLS detection
  options+=(--without-polarssl)          # disable PolarSSL detection
  options+=(--without-mbedtls)           # disable mbedTLS detection
  options+=(--without-cyassl)            # disable CyaSSL detection
  options+=(--without-wolfssl)           # disable WolfSSL detection
  options+=(--without-mesalink)          # disable MesaLink detection
  options+=(--without-nss)               # disable NSS detection
  options+=(--without-axtls)             # disable axTLS
  options+=(--without-libpsl)            # disable support for libpsl cookie checking
  options+=(--without-libmetalink)       # disable libmetalink detection
  options+=(--without-winidn)            # disable Windows native IDN

  autoconf_build $host "libcurl" ${options[@]}
}

function libcurl_get_deps {
  echo openssl
}
