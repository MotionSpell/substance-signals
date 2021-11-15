PLUG_DIR:=$(call get-my-dir)

TARGETS+=$(BIN)/HttpInput.smd
$(BIN)/HttpInput.smd: \
  $(BIN)/$(SRC)/lib_media/common/http_puller.cpp.o\
  $(BIN)/$(PLUG_DIR)/http_input.cpp.o\
