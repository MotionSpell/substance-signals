
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
  local NVIDIA=""
  if [ $ENABLE_NVIDIA == 1 ]; then
    NVIDIA="--enable-cuda-nvcc --enable-cuvid --enable-nonfree --enable-ffnvcodec  --enable-vdpau"
    XCOMPILE=""
  fi
  local FREETYPE2=""
  if [ $ENABLE_FREETYPE2 == "1" ]; then
    FREETYPE2="--enable-libfreetype"
  fi
  local PROGRAMS="--disable-programs"
  if [ $ENABLE_FFMPEG_PROGRAMS == 1 ]; then
    PROGRAMS=""
    XCOMPILE=""
  fi

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
      $PROGRAMS \
      --disable-gnutls \
      --disable-openssl \
      --disable-iconv \
      --disable-bzlib \
      --enable-avresample \
      --disable-decoder=mp3float \
      $FREETYPE2 \
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
  if [ $ENABLE_FREETYPE2 == "1" ]; then
    echo freetype2
  fi 
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


  local patchFile3=$scriptDir/patches/ffmpeg_03_aacdec_log_debug.diff
  cat << 'EOF' > $patchFile3
diff --git a/libavcodec/aacdec.c b/libavcodec/aacdec.c
index d17852d8ba..9597b7d6f1 100644
--- a/libavcodec/aacdec.c
+++ b/libavcodec/aacdec.c
@@ -317,7 +317,7 @@ static int latm_decode_audio_specific_config(struct LATMContext *latmctx,
         ac->oc[1].m4ac.chan_config != m4ac.chan_config) {
 
         if (latmctx->initialized) {
-            av_log(avctx, AV_LOG_INFO, "audio config changed (sample_rate=%d, chan_config=%d)\n", m4ac.sample_rate, m4ac.chan_config);
+            av_log(avctx, AV_LOG_DEBUG, "audio config changed (sample_rate=%d, chan_config=%d)\n", m4ac.sample_rate, m4ac.chan_config);
         } else {
             av_log(avctx, AV_LOG_DEBUG, "initializing latmctx\n");
         }
EOF
  applyPatch $patchFile3


  local patchFile4=$scriptDir/patches/ffmpeg_04_aacdec_log_debug.diff
  cat << 'EOF' > $patchFile4
diff --git a/libavformat/movenc.c b/libavformat/movenc.c
index e422bdd071..e765cf0253 100644
--- a/libavformat/movenc.c
+++ b/libavformat/movenc.c
@@ -100,6 +100,7 @@ static const AVOption options[] = {
     { "encryption_kid", "The media encryption key identifier (hex)", offsetof(MOVMuxContext, encryption_kid), AV_OPT_TYPE_BINARY, .flags = AV_OPT_FLAG_ENCODING_PARAM },
     { "use_stream_ids_as_track_ids", "use stream ids as track ids", offsetof(MOVMuxContext, use_stream_ids_as_track_ids), AV_OPT_TYPE_BOOL, {.i64 = 0}, 0, 1, AV_OPT_FLAG_ENCODING_PARAM},
     { "write_tmcd", "force or disable writing tmcd", offsetof(MOVMuxContext, write_tmcd), AV_OPT_TYPE_BOOL, {.i64 = -1}, -1, 1, AV_OPT_FLAG_ENCODING_PARAM},
+    { "ism_offset", "Offset to the ISM fragment start times", offsetof(MOVMuxContext, ism_offset), AV_OPT_TYPE_INT64, {.i64 = 0}, 0, INT64_MAX, AV_OPT_FLAG_ENCODING_PARAM},
     { "write_prft", "Write producer reference time box with specified time source", offsetof(MOVMuxContext, write_prft), AV_OPT_TYPE_INT, {.i64 = MOV_PRFT_NONE}, 0, MOV_PRFT_NB-1, AV_OPT_FLAG_ENCODING_PARAM, "prft"},
     { "wallclock", NULL, 0, AV_OPT_TYPE_CONST, {.i64 = MOV_PRFT_SRC_WALLCLOCK}, 0, 0, AV_OPT_FLAG_ENCODING_PARAM, "prft"},
     { "pts", NULL, 0, AV_OPT_TYPE_CONST, {.i64 = MOV_PRFT_SRC_PTS}, 0, 0, AV_OPT_FLAG_ENCODING_PARAM, "prft"},
@@ -6316,6 +6317,7 @@ static int mov_init(AVFormatContext *s)
          * this is updated. */
         track->hint_track = -1;
         track->start_dts  = AV_NOPTS_VALUE;
+        track->frag_start += mov->ism_offset;
         track->start_cts  = AV_NOPTS_VALUE;
         track->end_pts    = AV_NOPTS_VALUE;
         track->dts_shift  = AV_NOPTS_VALUE;
diff --git a/libavformat/movenc.h b/libavformat/movenc.h
index 68d6f23a5a..28766bcaf2 100644
--- a/libavformat/movenc.h
+++ b/libavformat/movenc.h
@@ -234,6 +234,8 @@ typedef struct MOVMuxContext {
     int write_tmcd;
     MOVPrftBox write_prft;
     int empty_hdlr_name;
+
+    int64_t ism_offset;
 } MOVMuxContext;
 
 #define FF_MOV_FLAG_RTP_HINT              (1 <<  0)
EOF
  applyPatch $patchFile4



  local patchFile5=$scriptDir/patches/ffmpeg_05_multiple_frames_in_packet_warning.diff
  cat << 'EOF' > $patchFile5
diff --git a/libavcodec/decode.c b/libavcodec/decode.c
index cd275bacc4..b27d746c12 100644
--- a/libavcodec/decode.c
+++ b/libavcodec/decode.c
@@ -564,7 +564,7 @@ FF_ENABLE_DEPRECATION_WARNINGS
         !avci->showed_multi_packet_warning &&
         ret >= 0 && ret != pkt->size && !(avctx->codec->capabilities & AV_CODEC_CAP_SUBFRAMES)) {
         av_log(avctx, AV_LOG_WARNING, "Multiple frames in a packet.\n");
-        avci->showed_multi_packet_warning = 1;
+        //avci->showed_multi_packet_warning = 1;
     }
 
     if (!got_frame)
EOF
  applyPatch $patchFile5
}
