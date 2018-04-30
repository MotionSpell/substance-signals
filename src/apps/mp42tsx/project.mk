MYDIR=$(call get-my-dir)
OUTDIR:=$(BIN)/$(MYDIR)
TARGET:=$(OUTDIR)/$(notdir $(MYDIR)).exe
TARGETS+=$(TARGET)
EXE_MP42TSX_OBJS:=\
	$(LIB_MEDIA_OBJS)\
	$(LIB_MODULES_OBJS)\
	$(LIB_PIPELINE_OBJS)\
	$(LIB_UTILS_OBJS)\
 	$(OUTDIR)/mp42tsx.o\
 	$(OUTDIR)/options.o\
 	$(OUTDIR)/pipeliner_mp42ts.o
$(TARGET): $(EXE_MP42TSX_OBJS)
DEPS+=$(EXE_MP42TSX_OBJS:%.o=%.deps)
