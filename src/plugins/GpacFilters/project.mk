PLUG_DIR:=$(call get-my-dir)

$(BIN)/GpacFilters.smd: PKGS+=gpac

TARGETS+=$(BIN)/GpacFilters.smd
$(BIN)/GpacFilters.smd: \
  $(BIN)/$(PLUG_DIR)/gpac_filters.cpp.o \
  $(BIN)/$(PLUG_DIR)/gpac_filter_mem_in.c.o \
  $(BIN)/$(PLUG_DIR)/gpac_filter_mem_out.c.o \

