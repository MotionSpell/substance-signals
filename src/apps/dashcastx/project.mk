MYDIR=$(call get-my-dir)
OUTDIR:=$(BIN)/$(MYDIR)
TARGET:=$(OUTDIR)/$(notdir $(MYDIR)).exe
TARGETS+=$(TARGET)
EXE_DASHCASTX_SRCS:=\
	$(LIB_MEDIA_SRCS)\
	$(LIB_MODULES_SRCS)\
	$(LIB_PIPELINE_SRCS)\
	$(LIB_UTILS_SRCS)\
	$(LIB_APPCOMMON_SRCS)\
	$(MYDIR)/main.cpp\
	$(MYDIR)/options.cpp\
	$(MYDIR)/pipeliner_dashcastx.cpp\

$(TARGET): $(EXE_DASHCASTX_SRCS:%=$(BIN)/%.o)
