OUTDIR:=$(BIN)/$(ProjectName)
TARGET:=$(OUTDIR)/$(notdir $(ProjectName)).exe
TARGETS+=$(TARGET)
EXE_DASHCASTX_OBJS:=\
	$(LIB_MEDIA_OBJS)\
	$(LIB_MODULES_OBJS)\
	$(LIB_PIPELINE_OBJS)\
	$(LIB_UTILS_OBJS)\
	$(LIB_APPCOMMON_OBJS)\
 	$(OUTDIR)/main.o\
 	$(OUTDIR)/options.o\
 	$(OUTDIR)/pipeliner.o
$(TARGET): $(EXE_DASHCASTX_OBJS)
DEPS+=$(EXE_DASHCASTX_OBJS:%.o=%.deps)
