MYDIR=$(call get-my-dir)
OUTDIR:=$(BIN)/$(call get-my-dir)
TESTOUTDIR:=$(CURDIR)/$(OUTDIR)

TEST_COMMON_SRCS:=\
	$(MYDIR)/tests.cpp

#---------------------------------------------------------------
# test_modules.exe
#---------------------------------------------------------------
EXE_MODULES_SRCS:=\
	$(TEST_COMMON_SRCS)\
	$(LIB_MEDIA_SRCS)\
	$(LIB_MODULES_SRCS)\
	$(LIB_UTILS_SRCS)

EXE_MODULES_SRCS+=$(MYDIR)/modules_player.cpp
EXE_MODULES_SRCS+=$(MYDIR)/modules_render.cpp
EXE_MODULES_SRCS+=$(MYDIR)/modules_generator.cpp
EXE_MODULES_SRCS+=$(MYDIR)/modules_converter.cpp
EXE_MODULES_SRCS+=$(MYDIR)/modules_decode.cpp
EXE_MODULES_SRCS+=$(MYDIR)/modules_demux.cpp
EXE_MODULES_SRCS+=$(MYDIR)/modules_encoder.cpp
EXE_MODULES_SRCS+=$(MYDIR)/modules_erasure.cpp
EXE_MODULES_SRCS+=$(MYDIR)/modules_metadata.cpp
EXE_MODULES_SRCS+=$(MYDIR)/modules_mux.cpp
EXE_MODULES_SRCS+=$(MYDIR)/modules_rectifier.cpp
EXE_MODULES_SRCS+=$(MYDIR)/modules_streamer.cpp
EXE_MODULES_SRCS+=$(MYDIR)/modules_timings.cpp
EXE_MODULES_SRCS+=$(MYDIR)/modules_transcoder.cpp


TARGETS+=$(OUTDIR)/test_modules.exe
$(OUTDIR)/test_modules.exe: $(EXE_MODULES_SRCS:%=$(BIN)/%.o)
TESTS+=$(TESTOUTDIR)/test_modules.exe
TESTS_DIR+=$(CURDIR)/$(SRC)/tests

#---------------------------------------------------------------
# test_other.exe : all the unit tests gathered from
# the in-tree 'unittests' directories.
#---------------------------------------------------------------
EXE_OTHER_SRCS:=\
	$(TEST_COMMON_SRCS)\
	$(LIB_MEDIA_SRCS)\
	$(LIB_MODULES_SRCS)\
	$(LIB_PIPELINE_SRCS)\
	$(LIB_UTILS_SRCS)

EXE_OTHER_SRCS+=$(shell find $(MYDIR)/.. -path "*/unittests/*.cpp")
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
