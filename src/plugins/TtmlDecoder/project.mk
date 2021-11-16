PLUG_DIR:=$(call get-my-dir)

TARGETS+=$(BIN)/TTMLDecoder.smd
$(BIN)/TTMLDecoder.smd: \
  $(BIN)/$(PLUG_DIR)/ttml_decoder.cpp.o\

