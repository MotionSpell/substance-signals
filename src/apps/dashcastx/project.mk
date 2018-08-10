MYDIR=$(call get-my-dir)

EXE_DASHCASTX_SRCS:=\
	$(MYDIR)/main.cpp\
	$(MYDIR)/pipeliner_dashcastx.cpp\
	$(MYDIR)/../../lib_appcommon/safemain.cpp\
	$(LIB_MEDIA_SRCS)\
	$(LIB_MODULES_SRCS)\
	$(LIB_PIPELINE_SRCS)\
	$(LIB_UTILS_SRCS)\
	$(LIB_APPCOMMON_SRCS)\

$(BIN)/dashcastx.exe: $(EXE_DASHCASTX_SRCS:%=$(BIN)/%.o)
TARGETS+=$(BIN)/dashcastx.exe

