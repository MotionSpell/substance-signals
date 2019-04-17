PLUG_DIR:=$(call get-my-dir)

TARGETS+=$(BIN)/HlsDemuxer.smd
$(BIN)/HlsDemuxer.smd: \
  $(BIN)/$(PLUG_DIR)/hls_demux.cpp.o\
  $(BIN)/$(SRC)/lib_media/common/http_puller.cpp.o\

