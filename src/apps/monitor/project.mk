MYDIR=$(call get-my-dir)

EXE_MONITOR_SRCS:=\
	$(MYDIR)/main.cpp\
	$(LIB_MODULES_SRCS)\
	$(LIB_PIPELINE_SRCS)\
	$(LIB_UTILS_SRCS)\
	$(LIB_APPCOMMON_SRCS)\

$(BIN)/monitor.exe: $(EXE_MONITOR_SRCS:%=$(BIN)/%.o)
TARGETS+=$(BIN)/monitor.exe


