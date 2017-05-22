OUTDIR:=$(BIN)/$(ProjectName)
TARGET:=$(OUTDIR)/$(notdir $(ProjectName)).exe
TARGETS+=$(TARGET)
EXE_PLAYER_OBJS:=\
	$(LIB_MEDIA_OBJS)\
	$(LIB_MODULES_OBJS)\
	$(LIB_PIPELINE_OBJS)\
	$(LIB_UTILS_OBJS)\
 	$(OUTDIR)/pipeliner.o\
 	$(OUTDIR)/player.o
$(TARGET): $(EXE_PLAYER_OBJS)
DEPS+=$(EXE_PLAYER_OBJS:%.o=%.deps)
