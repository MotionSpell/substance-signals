PLUG_DIR:=$(call get-my-dir)

TARGETS+=$(BIN)/MPEG_DASH.smd
$(BIN)/MPEG_DASH.smd: PKGS+=gpac
$(BIN)/MPEG_DASH.smd: \
  $(BIN)/$(PLUG_DIR)/mpeg_dash.cpp.o\

