MYDIR=$(call get-my-dir)
OUTDIR:=$(BIN)/$(call get-my-dir)

#---------------------------------------------------------------
# unittests.exe : all the unit tests gathered from
# the in-tree 'unittests' directories.
#---------------------------------------------------------------
EXE_OTHER_SRCS:=\
  $(MYDIR)/tests.cpp\
  $(MYDIR)/../lib_appcommon/options.cpp\
  $(LIB_MEDIA_SRCS)\
  $(LIB_MODULES_SRCS)\
  $(LIB_PIPELINE_SRCS)\
  $(LIB_UTILS_SRCS)

EXE_OTHER_SRCS+=$(shell find src -path "*/unittests/*.cpp" | sort)
TARGETS+=$(BIN)/unittests.exe
$(BIN)/unittests.exe: $(EXE_OTHER_SRCS:%=$(BIN)/%.o)
TESTS_DIR+=$(CURDIR)/$(SRC)/tests
