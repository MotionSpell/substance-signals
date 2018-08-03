function postgres_build {
  lazy_download "postgres.tar.bz2" "https://ftp.postgresql.org/pub/source/v10.4/postgresql-10.4.tar.bz2"
  lazy_extract "postgres.tar.bz2"

  # Workaround: out-of-tree mingw builds are broken (postgreSQL 10.4)
  # (it can't find libpqdll.def and other .def files).
  # Let the build system regenerate those files, where it can find them aftewards.
  find postgres -name "*.def" -delete

  CFLAGS="-I$PREFIX/include" \
  LDFLAGS="-L$PREFIX/lib -lz" \
  ac_cv_file__dev_urandom=yes \
  autoconf_build $host "postgres" \
    --without-openssl \
    "--without-readline" \
    "--with-system-tzdata=/usr/share/zoneinfo" || true

  # Workaround: mingw build always fails the first time,
  # (w32obj.o not found) but succeeds after resuming.
  # Ignore the return value of the above "autoconf_build",
  # and resumt the build ourselves.
  pushDir postgres/build/$host
  $MAKE
  $MAKE install
  popDir
}

function postgres_get_deps {
  echo zlib
}
