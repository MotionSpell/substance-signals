PLUG_DIR:=$(call get-my-dir)
PLUG_OS:=$(shell $(CXX) -dumpmachine | sed "s/.*-//")

-include $(PLUG_DIR)/$(PLUG_OS).mk

TARGETS+=$(BIN)/MulticastInput.smd
$(BIN)/MulticastInput.smd: \
  $(BIN)/$(PLUG_DIR)/multicast_input.cpp.o\
  $(MULTICASTINPUT_OS_SOCKET)\
