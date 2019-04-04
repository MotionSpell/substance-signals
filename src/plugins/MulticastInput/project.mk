PLUG_DIR:=$(call get-my-dir)

-include $(PLUG_DIR)/$(shell $(CXX) -dumpmachine | sed "s/.*-\([a-zA-Z]*\)[0-9.]*/\1/").mk

TARGETS+=$(BIN)/MulticastInput.smd
$(BIN)/MulticastInput.smd: \
  $(BIN)/$(PLUG_DIR)/multicast_input.cpp.o\
  $(MULTICASTINPUT_OS_SOCKET)\
