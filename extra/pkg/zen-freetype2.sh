
function freetype2_build {
  host=$1

  lazy_download "freetype2.tar.bz2" "http://download.savannah.gnu.org/releases/freetype/freetype-2.7.1.tar.bz2"
  lazy_extract "freetype2.tar.bz2"
  mkgit "freetype2"

  pushDir "freetype2"
  freetype2_patches
  popDir

  autoconf_build $host "freetype2" \
    "--without-png" \
    "--enable-shared" \
    "--disable-static"
}

function freetype2_get_deps {
  echo zlib
}

function freetype2_patches {
  local patchFile=$scriptDir/patches/freetype2_01_pkgconfig.diff
  cat << 'EOF' > $patchFile
diff --git a/builds/unix/freetype2.in b/builds/unix/freetype2.in
index c4dfda4..97f256e 100644
--- a/builds/unix/freetype2.in
+++ b/builds/unix/freetype2.in
@@ -7,7 +7,7 @@ Name: FreeType 2
 URL: http://freetype.org
 Description: A free, high-quality, and portable font engine.
 Version: %ft_version%
-Requires:
+Requires: %REQUIRES_PRIVATE%
 Requires.private: %REQUIRES_PRIVATE%
 Libs: -L${libdir} -lfreetype
 Libs.private: %LIBS_PRIVATE%
EOF

  applyPatch $patchFile
}

