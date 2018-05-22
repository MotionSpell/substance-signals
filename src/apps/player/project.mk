MYDIR=$(call get-my-dir)

EXE_PLAYER_SRCS:=\
	$(LIB_MEDIA_SRCS)\
	$(LIB_MODULES_SRCS)\
	$(LIB_PIPELINE_SRCS)\
	$(LIB_UTILS_SRCS)\
 	$(MYDIR)/pipeliner_player.cpp\
 	$(MYDIR)/player.cpp\

$(BIN)/player.exe: $(EXE_PLAYER_SRCS:%=$(BIN)/%.o)
TARGETS+=$(BIN)/player.exe
