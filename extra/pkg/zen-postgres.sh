function postgres_build {
  lazy_download "postgres.tar.bz2" "https://ftp.postgresql.org/pub/source/v10.4/postgresql-10.4.tar.bz2"
  lazy_extract "postgres.tar.bz2"

  ac_cv_file__dev_urandom=yes \
  autoconf_build $host "postgres" \
    "--without-readline" \
    "--with-system-tzdata=/usr/share/zoneinfo"
}

function postgres_get_deps {
  echo zlib
}
