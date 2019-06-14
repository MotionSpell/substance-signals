PLUG_DIR:=$(call get-my-dir)

TARGETS+=$(BIN)/MPEG_DASH.smd
$(BIN)/MPEG_DASH.smd: \
  $(BIN)/$(PLUG_DIR)/mpeg_dash.cpp.o\
  $(BIN)/$(PLUG_DIR)/mpd.cpp.o\
  $(BIN)/$(PLUG_DIR)/xml.cpp.o\

