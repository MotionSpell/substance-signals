MYDIR=$(call get-my-dir)
OUTDIR:=$(BIN)/$(MYDIR)

TARGET:=$(OUTDIR)/player.exe
TARGETS+=$(TARGET)

EXE_PLAYER_SRCS:=\
	$(LIB_MEDIA_SRCS)\
	$(LIB_MODULES_SRCS)\
	$(LIB_PIPELINE_SRCS)\
	$(LIB_UTILS_SRCS)\
 	$(MYDIR)/pipeliner_player.cpp\
 	$(MYDIR)/player.cpp\

$(TARGET): $(EXE_PLAYER_SRCS:%=$(BIN)/%.o)
