function postgres_build {
  lazy_download "postgres.tar.bz2" "https://ftp.postgresql.org/pub/source/v10.4/postgresql-10.4.tar.bz2"
  lazy_extract "postgres.tar.bz2"
  
  autoconf_build $host "freetype2" \
    "--with-openssl"
}

function postgres_get_deps {
  echo zlib
  echo openssl
}
