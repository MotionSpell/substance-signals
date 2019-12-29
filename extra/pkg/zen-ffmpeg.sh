
function ffmpeg_build {
  host=$1

  lazy_download "ffmpeg.tar.gz" "http://ffmpeg.org/releases/ffmpeg-4.2.1.tar.bz2"
  lazy_extract "ffmpeg.tar.gz"
  mkgit "ffmpeg"

  (
    cd ffmpeg
    ffmpeg_patches
  )

  local ARCH=$(get_arch $host)
  local OS=$(get_os $host)

  local os=$OS
  case $OS in
    darwin*)
      os="darwin"
      ;;
  esac

  # remove stupid dependency
  $sed -i "s/jack_jack_h pthreads/jack_jack_h/" ffmpeg/configure

  mkdir -p ffmpeg/build/$host
  pushDir ffmpeg/build/$host

  local X264=""
  if [ $ENABLE_X264 == "1" ]; then
    X264="--enable-gpl --enable-libx264"
  fi 
  

  # crosscompilation + nvidia aren't friends
  local XCOMPILE="--target-os=$os --arch=$ARCH --cross-prefix=$host-"
  local NVIDIA="--enable-cuda-nvcc --enable-cuvid --enable-nonfree --enable-ffnvcodec"
  if [ $ENABLE_NVIDIA == 1 ]; then
    XCOMPILE=""
    NVIDIA=""
  fi

  # --extra-cflags='-I/opt/cuda/include' --extra-ldflags="-L/opt/cuda/lib64" --enable-cuda-nvcc --enable-cuvid --enable-nonfree --enable-ffnvcodec
  CFLAGS="-I$PREFIX/include -I/opt/cuda/include" \
  LDFLAGS="-L$PREFIX/lib -L/opt/cuda/lib64" \
  ../../configure \
      --prefix=$PREFIX \
      --enable-pthreads \
      --disable-w32threads \
      --disable-debug \
      --disable-doc \
      --disable-static \
      --enable-shared \
      $X264 \
      $NVIDIA \
      --enable-libopenh264 \
      --enable-zlib \
      --disable-programs \
      --disable-gnutls \
      --disable-openssl \
      --disable-iconv \
      --disable-bzlib \
      --enable-avresample \
      --disable-decoder=mp3float \
      --pkg-config=pkg-config \
      $XCOMPILE
  $MAKE
  $MAKE install
  popDir
}

function ffmpeg_get_deps {
  if [ $ENABLE_NVIDIA == "1" ]; then
    echo ffnvenc
  fi 
  echo libpthread
  if [ $ENABLE_X264 == "1" ]; then
    echo x264
  fi 
  echo zlib
  echo openh264
}


function ffmpeg_patches {
  local patchFile1=$scriptDir/patches/ffmpeg_01_hlsenc_lower_verbosity.diff
  cat << 'EOF' > $patchFile1
diff --git a/libavformat/hlsenc.c b/libavformat/hlsenc.c
index c27a66e..6bfb175 100644
--- a/libavformat/hlsenc.c
+++ b/libavformat/hlsenc.c
@@ -2206,7 +2206,7 @@ static int hls_write_packet(AVFormatContext *s, AVPacket *pkt)
             if (pkt->duration) {
                 vs->duration += (double)(pkt->duration) * st->time_base.num / st->time_base.den;
             } else {
-                av_log(s, AV_LOG_WARNING, "pkt->duration = 0, maybe the hls segment duration will not precise\n");
+                av_log(s, AV_LOG_VERBOSE, "pkt->duration = 0, maybe the hls segment duration will not precise\n");
                 vs->duration = (double)(pkt->pts - vs->end_pts) * st->time_base.num / st->time_base.den;
             }
         }
EOF

  applyPatch $patchFile1

#   #with MPEG-TS input libavformat would assert:
#   #[libav-log::panic] Assertion len >= s->orig_buffer_size failed at src/libavformat/aviobuf.c:581
#   #see https://www.mail-archive.com/ffmpeg-devel@ffmpeg.org/msg36880.html
#   local patchFile2=$scriptDir/patches/ffmpeg_02_avio_mpegts_demux_assert.diff
#   cat << 'EOF' > $patchFile2
# diff --git a/libavformat/aviobuf.c b/libavformat/aviobuf.c
# index e752d0e..3344738 100644
# --- a/libavformat/aviobuf.c
# +++ b/libavformat/aviobuf.c
# @@ -578,8 +578,8 @@ static void fill_buffer(AVIOContext *s)

#              s->checksum_ptr = dst = s->buffer;
#          }
# -        av_assert0(len >= s->orig_buffer_size);
# -        len = s->orig_buffer_size;
# +        if (len >= s->orig_buffer_size);
# +            len = s->orig_buffer_size;
#      }

#      len = read_packet_wrapper(s, dst, len);
# EOF

#   applyPatch $patchFile2
}
