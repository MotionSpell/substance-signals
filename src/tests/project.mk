OUTDIR:=$(BIN)/$(ProjectName)

TEST_COMMON_OBJ:=\
	$(OUTDIR)/tests.o
DEPS+=$(TEST_COMMON_OBJ:%.o=%.deps)

$(BIN)/$(ProjectName)/signals_%.o: CFLAGS+=-DUNIT
$(BIN)/$(ProjectName)/modules_%.o: CFLAGS+=-DUNIT
$(BIN)/$(ProjectName)/utils_%.o: CFLAGS+=-DUNIT

#---------------------------------------------------------------
# utils.exe
#---------------------------------------------------------------
EXE_UTILS_OBJS:=\
	$(OUTDIR)/utils.o\
	$(OUTDIR)/utils_fifo.o\
	$(LIB_UTILS_OBJS)\
	$(TEST_COMMON_OBJ)
DEPS+=$(EXE_UTILS_OBJS:%.o=%.deps)
TARGETS+=$(OUTDIR)/test_utils.exe
$(OUTDIR)/test_utils.exe: $(EXE_UTILS_OBJS)

#---------------------------------------------------------------
# signals.exe
#---------------------------------------------------------------
EXE_SIGNALS_OBJS:=\
	$(OUTDIR)/signals.o\
	$(TEST_COMMON_OBJ)
DEPS+=$(EXE_SIGNALS_OBJS:%.o=%.deps)
TARGETS+=$(OUTDIR)/test_signals.exe
$(OUTDIR)/test_signals.exe: $(EXE_SIGNALS_OBJS)

#---------------------------------------------------------------
# modules.exe
#---------------------------------------------------------------
EXE_MODULES_OBJS:=\
	$(OUTDIR)/modules.o\
	$(TEST_COMMON_OBJ)\
	$(LIB_MEDIA_OBJS)\
	$(LIB_MODULES_OBJS)\
	$(LIB_UTILS_OBJS)
DEPS+=$(EXE_MODULES_OBJS:%.o=%.deps)
TARGETS+=$(OUTDIR)/test_modules.exe
$(OUTDIR)/test_modules.exe: $(EXE_MODULES_OBJS)

#---------------------------------------------------------------
# pipeline.exe
#---------------------------------------------------------------
EXE_PIPELINE_OBJS:=\
	$(OUTDIR)/pipeline.o\
	$(TEST_COMMON_OBJ)\
	$(LIB_MEDIA_OBJS)\
	$(LIB_MODULES_OBJS)\
	$(LIB_PIPELINE_OBJS)\
	$(LIB_UTILS_OBJS)
DEPS+=$(EXE_PIPELINE_OBJS:%.o=%.deps)
TARGETS+=$(OUTDIR)/test_pipeline.exe
$(OUTDIR)/test_pipeline.exe: $(EXE_PIPELINE_OBJS)

#---------------------------------------------------------------
# run tests
#---------------------------------------------------------------

TestProjectName:=$(ProjectName)
TestOutDir:=$(OUTDIR)

run: unit
	$(TestOutDir)/test_utils.exe
	$(TestOutDir)/test_signals.exe
	cd src/tests ; ../../$(TestOutDir)/test_modules.exe
	cd src/tests ; ../../$(TestOutDir)/test_pipeline.exe
