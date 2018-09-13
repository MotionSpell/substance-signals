
function gpac_build {
  host=$1

  # do not use a truncated hash here, use the full hash!
  # (collisions DO occur with truncated hashes, in practice this would
  # have the effect of stopping the whole build)
  readonly hash="b6f7409ce2ad68b6820dacb48430c25e0d0854a0"

  lazy_git_clone https://github.com/gpac/gpac.git gpac "$hash"

  (
  cd gpac
  gpac_apply_patches
  )

  local OS=$(get_os $host)
  local crossPrefix=$(get_cross_prefix $BUILD $host)

  local os=$OS
  case $OS in
    darwin*)
      os="Darwin"
      ;;
  esac

  mkdir -p gpac/build/$host
  pushDir gpac/build/$host

  ../../configure \
    --target-os=$os \
    --cross-prefix="$crossPrefix" \
    --extra-cflags="-I$PREFIX/include -w -fPIC" \
    --extra-ldflags="-L$PREFIX/lib" \
    --disable-jack \
    --disable-ssl \
    --use-png=no \
    --disable-player \
    --prefix=$PREFIX

  $MAKE lib
  $MAKE install # this is what causes gpac.pc to be copied to lib/pkg-config
  $MAKE install-lib

  popDir
}

function gpac_get_deps {
  echo libpthread
  echo ffmpeg
  echo freetype2
  echo libogg
  echo libsdl2
  echo libogg
  echo zlib
}

function gpac_apply_patches {
  cat << 'EOF' > fix.patch
diff --git a/configure b/configure
index f2f22157c..650c9a628 100755
--- a/configure
+++ b/configure
@@ -3461,16 +3461,20 @@ fi
 echo '	$(CXX) $(CFLAGS) -c -o $@ $<' >> config.mak

 #pkg-config
-echo "prefix=$prefix"				 > gpac.pc
-echo "exec_prefix=\${prefix}"			>> gpac.pc
-echo "libdir=\${exec_prefix}/$libdir"		>> gpac.pc
-echo "includedir=\${exec_prefix}/include"	>> gpac.pc
-echo ""						>> gpac.pc
-echo "Name: gpac"				>> gpac.pc
-echo "Description: GPAC Multimedia Framework"	>> gpac.pc
-echo "URL: http://gpac.sourceforge.net"		>> gpac.pc
-echo "Version:$version"				>> gpac.pc
-echo "Cflags: -I\${prefix}/include/gpac"	>> gpac.pc
-echo "Libs: -L\${libdir} -lgpac"		>> gpac.pc
+generate_pkgconfig () {
+	echo "prefix=$prefix"
+	echo "exec_prefix=\${prefix}"
+	echo "libdir=\${exec_prefix}/$libdir"
+	echo "includedir=\${exec_prefix}/include"
+	echo ""
+	echo "Name: gpac"
+	echo "Description: GPAC Multimedia Framework"
+	echo "URL: http://gpac.sourceforge.net"
+	echo "Version:$version"
+	echo "Cflags: -I\${prefix}/include"
+	echo "Libs: -L\${libdir} -lgpac"
+}
+
+generate_pkgconfig > gpac.pc

 echo "Done - type 'make help' for make info, 'make' to build"
EOF
  applyPatch fix.patch
}



