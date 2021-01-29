PLUG_DIR:=$(call get-my-dir)

TARGETS+=$(BIN)/MPEG_DASH.smd
$(BIN)/MPEG_DASH.smd: \
  $(BIN)/$(SRC)/lib_utils/xml.cpp.o\
  $(BIN)/$(PLUG_DIR)/mpeg_dash.cpp.o\
  $(BIN)/$(PLUG_DIR)/mpd.cpp.o\

