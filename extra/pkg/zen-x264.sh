
function x264_build {
  local host=$1
  local crossPrefix=$(get_cross_prefix $BUILD $host)

  lazy_git_clone "git://git.videolan.org/x264.git" x264 40bb56814e56ed342040bdbf30258aab39ee9e89

  (
    cd x264
    libx264_patches
  )

  case $host in
    *darwin*)
      RANLIB="" x264_do_build
      ;;
    *mingw*)
      x264_do_build --enable-win32thread
      ;;
    *)
      x264_do_build
      ;;
  esac
}

function x264_get_deps {
  echo "libpthread"
}

function x264_do_build {
  autoconf_build $host x264 \
    --enable-static \
    --enable-pic \
    --disable-gpl \
    --disable-cli \
    --enable-strip \
    --disable-avs \
    --disable-swscale \
    --disable-lavf \
    --disable-ffms \
    --disable-gpac \
    --disable-opencl \
    --cross-prefix=$crossPrefix \
    "$@"
}

function libx264_patches {
  local patchFile=$scriptDir/patches/libx264_01_remove_version_sei.diff
  cat << 'EOF' > $patchFile
diff --git a/encoder/encoder.c b/encoder/encoder.c
index 54d2e5a8..5e079a80 100644
--- a/encoder/encoder.c
+++ b/encoder/encoder.c
@@ -1978,12 +1978,15 @@ int x264_encoder_headers( x264_t *h, x264_nal_t **pp_nal, int *pi_nal )
     if( x264_nal_end( h ) )
         return -1;

+    if(0)
+    {
     /* identify ourselves */
     x264_nal_start( h, NAL_SEI, NAL_PRIORITY_DISPOSABLE );
     if( x264_sei_version_write( h, &h->out.bs ) )
         return -1;
     if( x264_nal_end( h ) )
         return -1;
+    }

     frame_size = x264_encoder_encapsulate_nals( h, 0 );
     if( frame_size < 0 )
EOF

  applyPatch $patchFile
}
