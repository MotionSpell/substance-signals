MYDIR=$(call get-my-dir)

LIB_MEDIA_SRCS:=\
  $(MYDIR)/common/libav.cpp\
  $(MYDIR)/common/gpac.cpp\
  $(MYDIR)/common/picture.cpp\
  $(MYDIR)/common/iso8601.cpp\
  $(MYDIR)/decode/jpegturbo_decode.cpp\
  $(MYDIR)/decode/decoder.cpp\
  $(MYDIR)/demux/gpac_demux_mp4_simple.cpp\
  $(MYDIR)/demux/gpac_demux_mp4_full.cpp\
  $(MYDIR)/demux/libav_demux.cpp\
  $(MYDIR)/demux/dash_demux.cpp\
  $(MYDIR)/encode/jpegturbo_encode.cpp\
  $(MYDIR)/encode/libav_encode.cpp\
  $(MYDIR)/in/file.cpp\
  $(MYDIR)/in/mpeg_dash_input.cpp\
  $(MYDIR)/in/sound_generator.cpp\
  $(MYDIR)/in/video_generator.cpp\
  $(MYDIR)/mux/gpac_mux_m2ts.cpp\
  $(MYDIR)/mux/gpac_mux_mp4.cpp\
  $(MYDIR)/mux/gpac_mux_mp4_mss.cpp\
  $(MYDIR)/mux/libav_mux.cpp\
  $(MYDIR)/out/file.cpp\
  $(MYDIR)/out/http.cpp\
  $(MYDIR)/out/null.cpp\
  $(MYDIR)/out/print.cpp\
  $(MYDIR)/stream/apple_hls.cpp\
  $(MYDIR)/stream/mpeg_dash.cpp\
  $(MYDIR)/stream/ms_hss.cpp\
  $(MYDIR)/stream/adaptive_streaming_common.cpp\
  $(MYDIR)/transform/audio_convert.cpp\
  $(MYDIR)/transform/audio_gap_filler.cpp\
  $(MYDIR)/transform/libavfilter.cpp\
  $(MYDIR)/transform/restamp.cpp\
  $(MYDIR)/transform/telx2ttml.cpp\
  $(MYDIR)/transform/time_rectifier.cpp\
  $(MYDIR)/transform/video_convert.cpp\
  $(MYDIR)/utils/comparator.cpp\
  $(MYDIR)/utils/recorder.cpp\
  $(MYDIR)/utils/repeater.cpp

PKGS+=\
  gpac\
  libavcodec\
  libavdevice\
  libavfilter\
  libavformat\
  libavutil\
  libcurl\
  libswresample\
  libswscale\
  libturbojpeg\

CFLAGS+=-fPIC

$(BIN)/%.smd: $(LIB_MODULES_SRCS:%=$(BIN)/%.o) $(LIB_UTILS_SRCS:%=$(BIN)/%.o)
	$(CXX) $(CFLAGS) -pthread -shared -Wl,--no-undefined -o "$@" $^ $(LDFLAGS)

ifeq ($(SIGNALS_HAS_X11), 1)

CFLAGS+=-DSIGNALS_HAS_X11

TARGETS+=$(BIN)/SDLVideo.smd
$(BIN)/$(SRC)/lib_media/render/sdl_video.cpp.o: CFLAGS+=$(shell sdl2-config --cflags)
$(BIN)/SDLVideo.smd: LDFLAGS+=$(shell pkg-config sdl2 --libs)
$(BIN)/SDLVideo.smd: $(MYDIR)/render/sdl_video.cpp

TARGETS+=$(BIN)/SDLAudio.smd
$(BIN)/$(SRC)/lib_media/render/sdl_audio.cpp.o: CFLAGS+=$(shell sdl2-config --cflags)
$(BIN)/SDLAudio.smd: LDFLAGS+=$(shell pkg-config sdl2 --libs)
$(BIN)/SDLAudio.smd: \
	$(BIN)/$(SRC)/lib_media/render/sdl_audio.cpp.o \
	$(BIN)/$(SRC)/lib_media/transform/audio_convert.cpp.o \
	$(BIN)/$(SRC)/lib_media/common/libav.cpp.o

endif

# Warning derogations. TODO: make this list empty
$(BIN)/$(SRC)/lib_media/common/libav.cpp.o: CFLAGS+=-Wno-deprecated-declarations
$(BIN)/$(SRC)/lib_media/demux/libav_demux.cpp.o: CFLAGS+=-Wno-deprecated-declarations
$(BIN)/$(SRC)/lib_media/mux/libav_mux.cpp.o: CFLAGS+=-Wno-deprecated-declarations
