PLUG_DIR:=$(call get-my-dir)

TARGETS+=$(BIN)/RegulatorMulti.smd
$(BIN)/RegulatorMulti.smd: \
  $(BIN)/$(PLUG_DIR)/regulator_multi.cpp.o\

