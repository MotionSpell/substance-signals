MYDIR=$(call get-my-dir)
OUTDIR:=$(BIN)/$(MYDIR)
TARGET:=$(OUTDIR)/$(notdir $(MYDIR)).exe
TARGETS+=$(TARGET)
EXE_MP42TSX_SRCS:=\
  $(LIB_MEDIA_SRCS)\
  $(LIB_MODULES_SRCS)\
  $(LIB_PIPELINE_SRCS)\
  $(LIB_UTILS_SRCS)\
  $(LIB_APPCOMMON_SRCS)\
  $(MYDIR)/mp42tsx.cpp\
  $(MYDIR)/options.cpp\
  $(MYDIR)/pipeliner_mp42ts.cpp\

$(TARGET): $(EXE_MP42TSX_SRCS:%=$(BIN)/%.o)

