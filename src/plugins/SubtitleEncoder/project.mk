PLUG_DIR:=$(call get-my-dir)

TARGETS+=$(BIN)/SubtitleEncoder.smd
$(BIN)/SubtitleEncoder.smd: \
  $(BIN)/$(PLUG_DIR)/subtitle_encoder.cpp.o\

