PLUG_DIR:=$(call get-my-dir)

TARGETS+=$(BIN)/TsDemuxer.smd
$(BIN)/TsDemuxer.smd: \
  $(BIN)/$(PLUG_DIR)/ts_demuxer.cpp.o\

