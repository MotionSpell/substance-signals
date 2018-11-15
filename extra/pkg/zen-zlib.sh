
function zlib_build {
  host=$1
  lazy_download "zlib-$host.tar.gz" "http://zlib.net/fossils/zlib-1.2.11.tar.gz"
  lazy_extract "zlib-$host.tar.gz"
  mkgit "zlib-$host"
  pushDir zlib-$host

  zlib_patches

  chmod +x ./configure
  CFLAGS="-w -fPIC" \
  CHOST=$host \
    ./configure \
    --prefix=$PREFIX
  $MAKE
  $MAKE install

  popDir
}

function zlib_get_deps {
  local a=0
}

function zlib_patches {
  local patchFile=$scriptDir/patches/zlib_01_nobypass.diff
  cat << 'EOF' > $patchFile
diff --git a/configure b/configure
index b77a8a8..c82aea1 100644
--- a/configure
+++ b/configure
@@ -191,10 +191,6 @@ if test "$gcc" -eq 1 && ($cc -c $test.c) >> configure.log 2>&1; then
   CYGWIN* | Cygwin* | cygwin* | OS/2*)
         EXE='.exe' ;;
   MINGW* | mingw*)
-# temporary bypass
-        rm -f $test.[co] $test $test$shared_ext
-        echo "Please use win32/Makefile.gcc instead." | tee -a configure.log
-        leave 1
         LDSHARED=${LDSHARED-"$cc -shared"}
         LDSHAREDLIBC=""
         EXE='.exe' ;;
EOF

  applyPatch $patchFile
}
