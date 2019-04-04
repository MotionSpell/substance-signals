$(BIN)/render-config.mk: $(SRC)/../scripts/configure
	@mkdir -p $(BIN)
	$(SRC)/../scripts/configure --scope RENDER_ sdl2 > "$@"

ifneq ($(MAKECMDGOALS),clean)
include $(BIN)/render-config.mk
endif

TARGETS+=$(BIN)/SDLVideo.smd
$(BIN)/$(SRC)/plugins/SdlRender/sdl_video.cpp.o: CFLAGS+=$(RENDER_CFLAGS)
$(BIN)/SDLVideo.smd: LDFLAGS+=$(RENDER_LDFLAGS)
$(BIN)/SDLVideo.smd: \
  $(BIN)/$(SRC)/plugins/SdlRender/sdl_video.cpp.o

TARGETS+=$(BIN)/SDLAudio.smd
$(BIN)/$(SRC)/plugins/SdlRender/sdl_audio.cpp.o: CFLAGS+=$(RENDER_CFLAGS)
$(BIN)/SDLAudio.smd: LDFLAGS+=$(RENDER_LDFLAGS)
$(BIN)/SDLAudio.smd: \
  $(BIN)/$(SRC)/plugins/SdlRender/sdl_audio.cpp.o \

