PLUG_DIR:=$(call get-my-dir)

TARGETS+=$(BIN)/HlsDemuxer.smd
$(BIN)/HlsDemuxer.smd: LDFLAGS+=$(MEDIA_LDFLAGS)
$(BIN)/HlsDemuxer.smd: CFLAGS+=$(MEDIA_CFLAGS)
$(BIN)/HlsDemuxer.smd: \
  $(BIN)/$(PLUG_DIR)/hls_demux.cpp.o\
  $(BIN)/$(SRC)/lib_media/common/http_puller.cpp.o\

