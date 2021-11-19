PLUG_DIR:=$(call get-my-dir)

-include $(PLUG_DIR)/$(shell $(CXX) -dumpmachine | sed "s/.*-\([a-zA-Z]*\)[0-9.]*/\1/" | sed "s/linux/gnu/").mk

TARGETS+=$(BIN)/SocketInput.smd
$(BIN)/SocketInput.smd: \
  $(BIN)/$(PLUG_DIR)/socket_input.cpp.o\
  $(SOCKETINPUT_OS_SOCKET)\
