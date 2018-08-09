MYDIR=$(call get-my-dir)

LIB_MEDIA_SRCS:=\
  $(MYDIR)/common/libav.cpp\
  $(MYDIR)/common/libav_init.cpp\
  $(MYDIR)/common/gpac.cpp\
  $(MYDIR)/common/picture.cpp\
  $(MYDIR)/common/iso8601.cpp\
  $(MYDIR)/demux/gpac_demux_mp4_simple.cpp\
  $(MYDIR)/demux/gpac_demux_mp4_full.cpp\
  $(MYDIR)/demux/libav_demux.cpp\
  $(MYDIR)/demux/dash_demux.cpp\
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
  $(MYDIR)/transform/avcc2annexb.cpp\
  $(MYDIR)/transform/audio_gap_filler.cpp\
  $(MYDIR)/transform/libavfilter.cpp\
  $(MYDIR)/transform/restamp.cpp\
  $(MYDIR)/transform/telx2ttml.cpp\
  $(MYDIR)/transform/time_rectifier.cpp\
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

ifeq ($(SIGNALS_HAS_X11), 1)
include $(MYDIR)/render/render.mk
endif

$(BIN)/media-config.mk: $(SRC)/../scripts/configure
	@mkdir -p $(BIN)
	$(SRC)/../scripts/configure libswresample libswscale libturbojpeg | sed 's/^CFLAGS/MEDIA_CFLAGS/g' | sed 's/^LDFLAGS/MEDIA_LDFLAGS/g'> "$@"

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
TARGETS+=$(BIN)/Decoder.smd
$(BIN)/Decoder.smd: LDFLAGS+=$(MEDIA_LDFLAGS)
$(BIN)/Decoder.smd: CFLAGS+=$(MEDIA_CFLAGS)
$(BIN)/Decoder.smd: \
  $(BIN)/$(SRC)/lib_media/decode/decoder.cpp.o\
  $(BIN)/$(SRC)/lib_media/common/libav.cpp.o\
  $(BIN)/$(SRC)/lib_media/common/picture.cpp.o\

#------------------------------------------------------------------------------

# Warning derogations. TODO: make this list empty
$(BIN)/$(SRC)/lib_media/common/libav.cpp.o: CFLAGS+=-Wno-deprecated-declarations
$(BIN)/$(SRC)/lib_media/demux/libav_demux.cpp.o: CFLAGS+=-Wno-deprecated-declarations
$(BIN)/$(SRC)/lib_media/mux/libav_mux.cpp.o: CFLAGS+=-Wno-deprecated-declarations
