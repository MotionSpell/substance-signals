PLUG_DIR:=$(call get-my-dir)

TARGETS+=$(BIN)/TsMuxer.smd
$(BIN)/TsMuxer.smd: LDFLAGS+=$(MEDIA_LDFLAGS)
$(BIN)/TsMuxer.smd: CFLAGS+=$(MEDIA_CFLAGS)
$(BIN)/TsMuxer.smd: \
  $(BIN)/$(PLUG_DIR)/mpegts_muxer.cpp.o\
  $(BIN)/$(SRC)/lib_media/common/libav.cpp.o\

