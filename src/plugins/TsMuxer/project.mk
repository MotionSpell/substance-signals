PLUG_DIR:=$(call get-my-dir)

TARGETS+=$(BIN)/TsMuxer.smd
$(BIN)/TsMuxer.smd: \
  $(BIN)/$(PLUG_DIR)/mpegts_muxer.cpp.o\
  $(BIN)/$(PLUG_DIR)/pes.cpp.o\
  $(BIN)/$(PLUG_DIR)/crc.cpp.o\

