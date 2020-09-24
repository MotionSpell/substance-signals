PLUG_DIR:=$(call get-my-dir)

TARGETS+=$(BIN)/TTMLEncoder.smd
$(BIN)/TTMLEncoder.smd: \
  $(BIN)/$(PLUG_DIR)/ttml_encoder.cpp.o\

