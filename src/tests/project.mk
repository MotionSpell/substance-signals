MYDIR=$(call get-my-dir)
OUTDIR:=$(BIN)/$(call get-my-dir)
TESTOUTDIR:=$(CURDIR)/$(OUTDIR)

#---------------------------------------------------------------
# test_other.exe : all the unit tests gathered from
# the in-tree 'unittests' directories.
#---------------------------------------------------------------
EXE_OTHER_SRCS:=\
	$(MYDIR)/tests.cpp\
	$(LIB_MEDIA_SRCS)\
	$(LIB_MODULES_SRCS)\
	$(LIB_PIPELINE_SRCS)\
	$(LIB_UTILS_SRCS)

EXE_OTHER_SRCS+=$(shell find src -path "*/unittests/*.cpp" | sort)
TARGETS+=$(OUTDIR)/test_other.exe
$(OUTDIR)/test_other.exe: $(EXE_OTHER_SRCS:%=$(BIN)/%.o)
TESTS+=$(TESTOUTDIR)/test_other.exe
TESTS_DIR+=$(CURDIR)/$(SRC)/tests

#---------------------------------------------------------------
# run tests
#---------------------------------------------------------------
pairup=$(if $1$2, cd $(firstword $1) ; $(firstword $2) || exit ; $(call pairup,$(wordlist 2,$(words $1),$1),$(wordlist 2,$(words $2),$2)))

run: targets
	$(call pairup, $(TESTS_DIR), $(TESTS))
