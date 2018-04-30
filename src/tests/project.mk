OUTDIR:=$(BIN)/$(call get-my-dir)
TESTOUTDIR:=$(CURDIR)/$(OUTDIR)

TEST_COMMON_OBJ:=\
	$(OUTDIR)/tests.o
DEPS+=$(TEST_COMMON_OBJ:%.o=%.deps)

#---------------------------------------------------------------
# test_utils.exe
#---------------------------------------------------------------
EXE_UTILS_OBJS:=\
	$(OUTDIR)/utils.o\
	$(LIB_UTILS_OBJS)\
	$(TEST_COMMON_OBJ)
DEPS+=$(EXE_UTILS_OBJS:%.o=%.deps)
TARGETS+=$(OUTDIR)/test_utils.exe
$(OUTDIR)/test_utils.exe: $(EXE_UTILS_OBJS)
TESTS+=$(TESTOUTDIR)/test_utils.exe
TESTS_DIR+=$(CURDIR)/$(SRC)/tests

#---------------------------------------------------------------
# test_signals.exe
#---------------------------------------------------------------
EXE_SIGNALS_OBJS:=\
	$(OUTDIR)/signals.o\
	$(TEST_COMMON_OBJ)
DEPS+=$(EXE_SIGNALS_OBJS:%.o=%.deps)
TARGETS+=$(OUTDIR)/test_signals.exe
$(OUTDIR)/test_signals.exe: $(EXE_SIGNALS_OBJS)
TESTS+=$(TESTOUTDIR)/test_signals.exe
TESTS_DIR+=$(CURDIR)/$(SRC)/tests

#---------------------------------------------------------------
# test_modules.exe
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
TESTS+=$(TESTOUTDIR)/test_modules.exe
TESTS_DIR+=$(CURDIR)/$(SRC)/tests

#---------------------------------------------------------------
# test_pipeline.exe
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
TESTS+=$(TESTOUTDIR)/test_pipeline.exe
TESTS_DIR+=$(CURDIR)/$(SRC)/tests

#---------------------------------------------------------------
# run tests
#---------------------------------------------------------------
pairup=$(if $1$2, cd $(firstword $1) ; $(firstword $2) || exit ; $(call pairup,$(wordlist 2,$(words $1),$1),$(wordlist 2,$(words $2),$2)))

run: targets
	$(call pairup, $(TESTS_DIR), $(TESTS))
