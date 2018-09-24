PLUG_DIR:=$(call get-my-dir)
PLUG_OS:=$(shell $(CXX) -dumpmachine | sed "s/.*-//")

TARGETS+=$(BIN)/MulticastInput.smd
$(BIN)/MulticastInput.smd: \
  $(BIN)/$(PLUG_DIR)/multicast_input.cpp.o\
  $(BIN)/$(PLUG_DIR)/socket_$(PLUG_OS).cpp.o\

-include $(PLUG_DIR)/$(PLUG_OS).mk
