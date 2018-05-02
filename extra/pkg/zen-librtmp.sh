
function librtmp_build {
  host=$1
  lazy_git_clone "git://git.ffmpeg.org/rtmpdump" rtmpdump 79459a2b43f41ac44a2ec001139bcb7b1b8f7497

  pushDir rtmpdump/librtmp
  if [ $(uname -s) == "Darwin" ]; then
    librtmp_patches
  fi

  case $host in
    *mingw*)
      $sed -i "s/^SYS=posix/SYS=mingw/" Makefile
      echo "# YO" >> Makefile
      ;;
  esac

  $sed -i "s@^prefix=.*@prefix=$PREFIX/$host@" Makefile
  $sed -i "s@^CRYPTO=.*@@" Makefile

  $MAKE CROSS_COMPILE="$host-"
  $MAKE CROSS_COMPILE="$host-" install

  popDir
}

function librtmp_get_deps {
  local a=0
}

function librtmp_patches {
  local patchFile=$scriptDir/patches/librtmp_01_dylib_install_name.diff
  cat << 'EOF' > $patchFile
--- a/Makefile	2015-09-09 13:29:23.000000000 +0200
+++ b/Makefile	2015-09-09 13:30:34.000000000 +0200
@@ -53,7 +53,7 @@
 SODIR_mingw=$(BINDIR)
 SODIR=$(SODIR_$(SYS))

-SO_LDFLAGS_posix=-shared -Wl,-soname,$@
+SO_LDFLAGS_posix=-shared -Wl,-dylib_install_name,$@
 SO_LDFLAGS_darwin=-dynamiclib -twolevel_namespace -undefined dynamic_lookup \
 	-fno-common -headerpad_max_install_names -install_name $(libdir)/$@
 SO_LDFLAGS_mingw=-shared -Wl,--out-implib,librtmp.dll.a
EOF

  applyPatch $patchFile
}


