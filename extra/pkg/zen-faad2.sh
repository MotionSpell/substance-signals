
function faad2_build {
  host=$1
  pushDir $WORK/src

  lazy_download "faad2.tar.bz2" "http://downloads.sourceforge.net/faac/faad2-2.7.tar.bz2"
  lazy_extract "faad2.tar.bz2"
  mkgit "faad2"

  pushDir "faad2"
  faad2_patches
  popDir

  autoconf_build $host "faad2"

  popDir
}

function faad2_get_deps {
  echo ""
}

function faad2_patches {
  local patchFile=$scriptDir/patches/faad2_01_ErrorDeclSpecifier.diff
  cat << 'EOF' > $patchFile
diff --git a/frontend/main.c b/frontend/main.c
--- a/frontend/main.c
+++ b/frontend/main.c
@@ -31,7 +31,9 @@
 #ifdef _WIN32
 #define WIN32_LEAN_AND_MEAN
 #include <windows.h>
+#ifndef __MINGW32__
 #define off_t __int64
+#endif
 #else
 #include <time.h>
 #endif
EOF

  applyPatch $patchFile
}

