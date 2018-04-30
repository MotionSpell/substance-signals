MYDIR=$(call get-my-dir)
OUTDIR:=$(BIN)/$(MYDIR)
TARGET:=$(OUTDIR)/$(notdir $(MYDIR)).exe
TARGETS+=$(TARGET)
EXE_DASHCASTX_OBJS:=\
	$(LIB_MEDIA_OBJS)\
	$(LIB_MODULES_OBJS)\
	$(LIB_PIPELINE_OBJS)\
	$(LIB_UTILS_OBJS)\
	$(LIB_APPCOMMON_OBJS)\
 	$(OUTDIR)/main.o\
 	$(OUTDIR)/options.o\
 	$(OUTDIR)/pipeliner_dashcastx.o
$(TARGET): $(EXE_DASHCASTX_OBJS)
