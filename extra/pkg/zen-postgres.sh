function postgres_build {
  lazy_download "postgres.tar.bz2" "https://ftp.postgresql.org/pub/source/v10.4/postgresql-10.4.tar.bz2"
  lazy_extract "postgres.tar.bz2"

  # Workaround: out-of-tree mingw builds are broken (postgreSQL 10.4)
  # (it can't find libpqdll.def and other .def files).
  # Let the build system regenerate those files, where it can find them aftewards.
  find postgres -name "*.def" -delete

  mkdir -p postgres/build/$host
  pushDir postgres/build/$host

  CFLAGS="-I$PREFIX/include" \
  LDFLAGS="-L$PREFIX/lib" \
  ac_cv_file__dev_urandom=yes \
  ../../configure \
    --build=$BUILD \
    --host=$host \
    --prefix=$PREFIX \
    --without-openssl \
    --without-zlib \
    "--without-readline" \
    "--with-system-tzdata=/usr/share/zoneinfo"

  # only build the strict minimum for postgresql client
  $MAKE -C src/include
  $MAKE -C src/common
  $MAKE -C src/interfaces
  $MAKE -C src/interfaces install
  cp ../../src/include/postgres_ext.h $PREFIX/include
  cp src/include/pg_config_ext.h $PREFIX/include

  popDir
}

function postgres_get_deps {
  local a=0
}
