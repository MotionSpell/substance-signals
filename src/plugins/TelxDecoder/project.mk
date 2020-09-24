PLUG_DIR:=$(call get-my-dir)

TARGETS+=$(BIN)/TeletextDecoder.smd
$(BIN)/TeletextDecoder.smd: \
  $(BIN)/$(PLUG_DIR)/telx_decoder.cpp.o\
  $(BIN)/$(PLUG_DIR)/telx.cpp.o\

