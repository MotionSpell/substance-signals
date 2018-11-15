
function gpac_build {
  host=$1

  # do not use a truncated hash here, use the full hash!
  # (collisions DO occur with truncated hashes, in practice this would
  # have the effect of stopping the whole build)
  readonly hash="5c441ee30db7d497e0a23779fa165c81b5de6401"

  lazy_git_clone https://github.com/gpac/gpac.git gpac "$hash"

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
  echo libsdl2
  echo libogg
  echo zlib
}
