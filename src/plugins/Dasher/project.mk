PLUG_DIR:=$(call get-my-dir)

TARGETS+=$(BIN)/MPEG_DASH.smd
$(BIN)/MPEG_DASH.smd: \
  $(BIN)/$(SRC)/lib_media/common/xml.cpp.o\
  $(BIN)/$(PLUG_DIR)/mpeg_dash.cpp.o\
  $(BIN)/$(PLUG_DIR)/mpd.cpp.o\

