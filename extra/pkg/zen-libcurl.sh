function libcurl_build {
  lazy_download "libcurl.tar.gz" "https://curl.haxx.se/download/curl-7.62.0.tar.bz2"
  lazy_extract "libcurl.tar.gz"

  autoconf_build $host "libcurl"
}

function libcurl_get_deps {
  echo openssl
}
