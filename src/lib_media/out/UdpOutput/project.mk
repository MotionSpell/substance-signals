PLUG_DIR:=$(call get-my-dir)

-include $(PLUG_DIR)/$(shell $(CXX) -dumpmachine | sed "s/.*-\([a-zA-Z]*\)[0-9.]*/\1/").mk

TARGETS+=$(BIN)/UdpOutput.smd
$(BIN)/UdpOutput.smd: \
  $(BIN)/$(PLUG_DIR)/udp_output.cpp.o\
  $(MULTICASTINPUT_OS_SOCKET)\
