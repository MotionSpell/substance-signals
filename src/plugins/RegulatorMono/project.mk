PLUG_DIR:=$(call get-my-dir)

TARGETS+=$(BIN)/RegulatorMono.smd
$(BIN)/RegulatorMono.smd: \
  $(BIN)/$(PLUG_DIR)/regulator_mono.cpp.o\

