MYDIR=$(call get-my-dir)

EXE_MCASTDUMP_SRCS:=\
  $(LIB_MEDIA_SRCS)\
  $(LIB_MODULES_SRCS)\
  $(LIB_PIPELINE_SRCS)\
  $(LIB_UTILS_SRCS)\
  $(LIB_APPCOMMON_SRCS)\
  $(MYDIR)/main.cpp\

$(BIN)/ts2ip.exe: $(EXE_MCASTDUMP_SRCS:%=$(BIN)/%.o)
TARGETS+=$(BIN)/ts2ip.exe
