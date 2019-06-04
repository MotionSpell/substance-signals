PLUG_DIR:=$(call get-my-dir)

TARGETS+=$(BIN)/Fmp4Splitter.smd
$(BIN)/Fmp4Splitter.smd: \
  $(BIN)/$(PLUG_DIR)/fmp4_splitter.cpp.o\

