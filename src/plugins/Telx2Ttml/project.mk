PLUG_DIR:=$(call get-my-dir)

TARGETS+=$(BIN)/TeletextToTTML.smd
$(BIN)/TeletextToTTML.smd: \
  $(BIN)/$(PLUG_DIR)/telx2ttml.cpp.o\

