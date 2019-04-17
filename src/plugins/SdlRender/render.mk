
TARGETS+=$(BIN)/SDLVideo.smd
$(BIN)/SDLVideo.smd: PKGS+=sdl2
$(BIN)/SDLVideo.smd: \
  $(BIN)/$(SRC)/plugins/SdlRender/sdl_video.cpp.o

TARGETS+=$(BIN)/SDLAudio.smd
$(BIN)/SDLAudio.smd: PKGS+=sdl2
$(BIN)/SDLAudio.smd: \
  $(BIN)/$(SRC)/plugins/SdlRender/sdl_audio.cpp.o \

