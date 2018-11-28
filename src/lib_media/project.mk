MYDIR=$(call get-my-dir)

LIB_MEDIA_SRCS:=\
  $(MYDIR)/common/libav.cpp\
  $(MYDIR)/common/libav_init.cpp\
  $(MYDIR)/common/picture.cpp\
  $(MYDIR)/common/iso8601.cpp\
  $(MYDIR)/demux/gpac_demux_mp4_simple.cpp\
  $(MYDIR)/demux/gpac_demux_mp4_full.cpp\
  $(MYDIR)/demux/dash_demux.cpp\
  $(MYDIR)/in/file.cpp\
  $(MYDIR)/in/mpeg_dash_input.cpp\
  $(MYDIR)/in/sound_generator.cpp\
  $(MYDIR)/in/video_generator.cpp\
  $(MYDIR)/mux/gpac_mux_mp4.cpp\
  $(MYDIR)/mux/gpac_mux_mp4_mss.cpp\
  $(MYDIR)/out/file.cpp\
  $(MYDIR)/out/http.cpp\
  $(MYDIR)/out/null.cpp\
  $(MYDIR)/out/print.cpp\
  $(MYDIR)/stream/apple_hls.cpp\
  $(MYDIR)/stream/hls_muxer_libav.cpp\
  $(MYDIR)/stream/mpeg_dash.cpp\
  $(MYDIR)/stream/ms_hss.cpp\
  $(MYDIR)/stream/adaptive_streaming_common.cpp\
  $(MYDIR)/transform/avcc2annexb.cpp\
  $(MYDIR)/transform/audio_gap_filler.cpp\
  $(MYDIR)/transform/restamp.cpp\
  $(MYDIR)/transform/telx2ttml.cpp\
  $(MYDIR)/transform/time_rectifier.cpp\
  $(MYDIR)/utils/recorder.cpp\
  $(MYDIR)/utils/repeater.cpp

PKGS+=\
  gpac\
  libavcodec\
  libavfilter\
  libavutil\
  libcurl\

ifeq ($(SIGNALS_HAS_X11), 1)
include $(MYDIR)/render/render.mk
endif

$(BIN)/media-config.mk: $(SRC)/../scripts/configure
	@mkdir -p $(BIN)
	$(SRC)/../scripts/configure --scope MEDIA_ libswresample libavdevice libavformat libswscale libturbojpeg > "$@"

ifneq ($(MAKECMDGOALS),clean)
include $(BIN)/media-config.mk
endif

#------------------------------------------------------------------------------
TARGETS+=$(BIN)/VideoConvert.smd
$(BIN)/VideoConvert.smd: LDFLAGS+=$(MEDIA_LDFLAGS)
$(BIN)/VideoConvert.smd: CFLAGS+=$(MEDIA_CFLAGS)
$(BIN)/VideoConvert.smd: \
  $(BIN)/$(SRC)/lib_media/transform/video_convert.cpp.o\
  $(BIN)/$(SRC)/lib_media/common/libav.cpp.o\
  $(BIN)/$(SRC)/lib_media/common/picture.cpp.o\

#------------------------------------------------------------------------------
TARGETS+=$(BIN)/AudioConvert.smd
$(BIN)/AudioConvert.smd: LDFLAGS+=$(MEDIA_LDFLAGS)
$(BIN)/AudioConvert.smd: CFLAGS+=$(MEDIA_CFLAGS)
$(BIN)/AudioConvert.smd: \
  $(BIN)/$(SRC)/lib_media/transform/audio_convert.cpp.o\
  $(BIN)/$(SRC)/lib_media/common/libav.cpp.o\

#------------------------------------------------------------------------------
TARGETS+=$(BIN)/JPEGTurboDecode.smd
$(BIN)/JPEGTurboDecode.smd: LDFLAGS+=$(MEDIA_LDFLAGS)
$(BIN)/JPEGTurboDecode.smd: CFLAGS+=$(MEDIA_CFLAGS)
$(BIN)/JPEGTurboDecode.smd: \
  $(BIN)/$(SRC)/lib_media/decode/jpegturbo_decode.cpp.o\
  $(BIN)/$(SRC)/lib_media/common/picture.cpp.o\

#------------------------------------------------------------------------------
TARGETS+=$(BIN)/JPEGTurboEncode.smd
$(BIN)/JPEGTurboEncode.smd: LDFLAGS+=$(MEDIA_LDFLAGS)
$(BIN)/JPEGTurboEncode.smd: CFLAGS+=$(MEDIA_CFLAGS)
$(BIN)/JPEGTurboEncode.smd: \
  $(BIN)/$(SRC)/lib_media/encode/jpegturbo_encode.cpp.o\
  $(BIN)/$(SRC)/lib_media/common/picture.cpp.o\

#------------------------------------------------------------------------------
TARGETS+=$(BIN)/Encoder.smd
$(BIN)/Encoder.smd: LDFLAGS+=$(MEDIA_LDFLAGS)
$(BIN)/Encoder.smd: CFLAGS+=$(MEDIA_CFLAGS)
$(BIN)/Encoder.smd: \
  $(BIN)/$(SRC)/lib_media/encode/libav_encode.cpp.o\
  $(BIN)/$(SRC)/lib_media/common/libav.cpp.o\
  $(BIN)/$(SRC)/lib_media/common/picture.cpp.o\

#------------------------------------------------------------------------------
TARGETS+=$(BIN)/Decoder.smd
$(BIN)/Decoder.smd: LDFLAGS+=$(MEDIA_LDFLAGS)
$(BIN)/Decoder.smd: CFLAGS+=$(MEDIA_CFLAGS)
$(BIN)/Decoder.smd: \
  $(BIN)/$(SRC)/lib_media/decode/decoder.cpp.o\
  $(BIN)/$(SRC)/lib_media/common/libav.cpp.o\
  $(BIN)/$(SRC)/lib_media/common/picture.cpp.o\

#------------------------------------------------------------------------------
TARGETS+=$(BIN)/LibavDemux.smd
$(BIN)/LibavDemux.smd: LDFLAGS+=$(MEDIA_LDFLAGS)
$(BIN)/LibavDemux.smd: CFLAGS+=$(MEDIA_CFLAGS)
$(BIN)/LibavDemux.smd: \
  $(BIN)/$(SRC)/lib_media/demux/libav_demux.cpp.o\
  $(BIN)/$(SRC)/lib_media/common/libav.cpp.o\
  $(BIN)/$(SRC)/lib_media/transform/restamp.cpp.o\

#------------------------------------------------------------------------------
TARGETS+=$(BIN)/LibavMux.smd
$(BIN)/LibavMux.smd: LDFLAGS+=$(MEDIA_LDFLAGS)
$(BIN)/LibavMux.smd: CFLAGS+=$(MEDIA_CFLAGS)
$(BIN)/LibavMux.smd: \
  $(BIN)/$(SRC)/lib_media/mux/libav_mux.cpp.o\
  $(BIN)/$(SRC)/lib_media/common/libav.cpp.o\

#------------------------------------------------------------------------------
TARGETS+=$(BIN)/LibavFilter.smd
$(BIN)/LibavFilter.smd: LDFLAGS+=$(MEDIA_LDFLAGS)
$(BIN)/LibavFilter.smd: CFLAGS+=$(MEDIA_CFLAGS)
$(BIN)/LibavFilter.smd: \
  $(BIN)/$(SRC)/lib_media/transform/libavfilter.cpp.o\
  $(BIN)/$(SRC)/lib_media/common/picture.cpp.o\
  $(BIN)/$(SRC)/lib_media/common/libav.cpp.o\

#------------------------------------------------------------------------------
TARGETS+=$(BIN)/FileSystemSink.smd
$(BIN)/FileSystemSink.smd: \
  $(BIN)/$(SRC)/lib_media/out/filesystem.cpp.o\

#------------------------------------------------------------------------------
include $(SRC)/lib_media/in/MulticastInput/project.mk
include $(SRC)/lib_media/demux/TsDemuxer/project.mk

#------------------------------------------------------------------------------
# Warning derogations. TODO: make this list empty
$(BIN)/$(SRC)/lib_media/demux/libav_demux.cpp.o: CFLAGS+=-Wno-deprecated-declarations
