MYDIR=$(call get-my-dir)
OUTDIR:=$(BIN)/$(MYDIR)
TARGET:=$(OUTDIR)/$(notdir $(MYDIR)).exe
TARGETS+=$(TARGET)
EXE_PLAYER_OBJS:=\
	$(LIB_MEDIA_OBJS)\
	$(LIB_MODULES_OBJS)\
	$(LIB_PIPELINE_OBJS)\
	$(LIB_UTILS_OBJS)\
 	$(OUTDIR)/pipeliner_player.o\
 	$(OUTDIR)/player.o
$(TARGET): $(EXE_PLAYER_OBJS)
