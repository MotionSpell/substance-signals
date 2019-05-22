
function gpac_build {
  host=$1

  # do not use a truncated hash here, use the full hash!
  # (collisions DO occur with truncated hashes, in practice this would
  # have the effect of stopping the whole build)
  readonly hash="5c441ee30db7d497e0a23779fa165c81b5de6401"

  lazy_git_clone https://github.com/gpac/gpac.git gpac "$hash"

  (
    cd gpac
    gpac_patches
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

  local options=()

  options+=(--disable-player)

  # Can't use --disable-all, because it sets GPAC_DISABLE_CORE_TOOLS
  # options+=(--disable-all)           # disables all features in libgpac

  options+=(--disable-ssl)           # disable OpenSSL support
  options+=(--disable-alsa)          # disable Alsa audio
  options+=(--disable-jack)          # disable Jack audio
  options+=(--disable-oss-audio)     # disable OSS audio
  options+=(--disable-pulseaudio)    # disable Pulse audio

  options+=(--use-png=no)
  options+=(--use-jpeg=no)

  options+=(--disable-3d)            # disable 3D rendering
  options+=(--disable-atsc)          # disable ATSC3 support
  options+=(--disable-avi)           # disable AVI
  options+=(--disable-bifs)          # disable BIFS
  options+=(--disable-bifs-enc)      # disable BIFS coder
  options+=(--disable-crypt)         # disable crypto tools
  options+=(--disable-dvb4linux)     # disable dvb4linux support
  options+=(--disable-dvbx)          # disable DVB-specific tools (MPE, FEC, DSM-CC)
  options+=(--disable-ipv6)          # disable IPV6 support
  options+=(--disable-laser)         # disable LASeR coder
  options+=(--disable-loader-bt)     # disable scene loading from ISO File Format
  options+=(--disable-loader-isoff)  # disable scene loading from ISO File Format
  options+=(--disable-loader-xmt)    # disable scene loading from ISO File Format
  options+=(--disable-m2ps)          # disable MPEG2 PS
  options+=(--disable-mcrypt)        # disable MCRYPT support
  options+=(--disable-od-dump)       # disable OD dump
  options+=(--disable-odf)           # disable full support of MPEG-4 OD Framework
  options+=(--disable-ogg)           # disable OGG
  options+=(--disable-opt)           # disable GCC optimizations
  options+=(--disable-platinum)      # disable Platinum UPnP support
  options+=(--disable-qtvr)          # disable import of Cubic QTVR files
  options+=(--disable-saf)           # disable SAF container
  options+=(--disable-scene-dump)    # disable scene graph dump
  options+=(--disable-scene-encode)  # disable BIFS & LASeR to ISO File Format encoding
  options+=(--disable-scene-stats)   # disable scene graph statistics
  options+=(--disable-scenegraph)    # disable scenegraph, scene parsers and player (terminal and compositor)
  options+=(--disable-seng)          # disable scene encoder engine
  options+=(--disable-sman)          # disable scene manager
  options+=(--disable-streaming)     # disable RTP/RTSP/SDP
  options+=(--disable-svg)           # disable SVG
  options+=(--disable-swf)           # disable SWF import
  options+=(--disable-vobsub)        # disable VobSub support
  options+=(--disable-vrml)          # disable MPEG-4/VRML/X3D
  options+=(--disable-wx)            # disable wxWidgets support
  options+=(--disable-x11)           # disable X11
  options+=(--disable-x11-shm)       # disable X11 shared memory support
  options+=(--disable-x11-xv)        # disable X11 Xvideo support
  options+=(--disable-x3d)           # disable X3D only

  # Features that we actually use
  options+=(--enable-export)        # enable media exporters
  options+=(--enable-import)        # enable media importers
  options+=(--enable-parsers)       # enable AV parsers
  options+=(--enable-m2ts)          # enable MPEG2 TS
  options+=(--enable-m2ts-mux)      # enable MPEG2 TS Multiplexer
  options+=(--enable-ttxt)          # enable TTXT (3GPP / MPEG-4 Timed Text) support
  options+=(--enable-hevc)          # enable HEVC support
  options+=(--enable-isoff)         # enable ISO File Format
  options+=(--enable-isoff-frag)    # enable fragments in ISO File Format
  options+=(--enable-isoff-hds)     # enable HDS support in ISO File Format
  options+=(--enable-isoff-hint)    # enable ISO File Format hinting
  options+=(--enable-isoff-write)   # enable ISO File Format edit/write

  # We don't use 'isom-dump', however GPAC will generate an unlinkable libgpac.so otherwise
  options+=(--enable-isom-dump)     # enable ISOM dump

  ../../configure \
    --target-os=$os \
    --cross-prefix="$crossPrefix" \
    --extra-cflags="-I$PREFIX/include -w -fPIC" \
    --extra-ldflags="-L$PREFIX/lib" \
    --verbose \
    --prefix=$PREFIX \
    ${options[@]}

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
  echo libogg
  echo zlib
}

function gpac_patches {
  local patchFile=$scriptDir/patches/gpac_01_export_gf_mpd_write.diff
  cat << 'EOF' > $patchFile
diff --git a/include/gpac/internal/mpd.h b/include/gpac/internal/mpd.h
index ea36ac6..eff465b 100644
--- a/include/gpac/internal/mpd.h
+++ b/include/gpac/internal/mpd.h
@@ -396,6 +396,7 @@ void gf_mpd_segment_base_free(void *ptr);
 
 void gf_mpd_period_free(void *_item);
 
+GF_Err gf_mpd_write(GF_MPD const * const mpd, FILE *out);
 GF_Err gf_mpd_write_file(GF_MPD const * const mpd, const char *file_name);
 
 
diff --git a/src/export.cpp b/src/export.cpp
index aac037e..8c8648c 100644
--- a/src/export.cpp
+++ b/src/export.cpp
@@ -1997,6 +1997,7 @@
 #pragma comment (linker, EXPORT_SYMBOL(gf_mpd_solve_segment_list_xlink) )
 #pragma comment (linker, EXPORT_SYMBOL(gf_mpd_delete_segment_list) )
 #pragma comment (linker, EXPORT_SYMBOL(gf_m3u8_parse_master_playlist) )
+#pragma comment (linker, EXPORT_SYMBOL(gf_mpd_write) )
 #pragma comment (linker, EXPORT_SYMBOL(gf_mpd_write_file) )
 #pragma comment (linker, EXPORT_SYMBOL(gf_mpd_get_base_url_count) )
 #pragma comment (linker, EXPORT_SYMBOL(gf_mpd_resolve_url) )
diff --git a/src/media_tools/mpd.c b/src/media_tools/mpd.c
index 3a75450..89ff7ee 100644
--- a/src/media_tools/mpd.c
+++ b/src/media_tools/mpd.c
@@ -2351,7 +2351,8 @@ static void gf_mpd_print_period(GF_MPD_Period const * const period, Bool is_dyna
 
 }
 
-static GF_Err gf_mpd_write(GF_MPD const * const mpd, FILE *out)
+GF_EXPORT
+GF_Err gf_mpd_write(GF_MPD const * const mpd, FILE *out)
 {
 	u32 i;
 	GF_MPD_ProgramInfo *info;
EOF

  applyPatch $patchFile
}


