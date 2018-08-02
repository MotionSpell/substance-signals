$(BIN)/render-config.mk: $(SRC)/../scripts/configure
	@mkdir -p $(BIN)
	$(SRC)/../scripts/configure sdl2 | sed 's/^CFLAGS/RENDER_CFLAGS/g' | sed 's/^LDFLAGS/RENDER_LDFLAGS/g'> "$@"

ifneq ($(MAKECMDGOALS),clean)
include $(BIN)/render-config.mk
endif

TARGETS+=$(BIN)/SDLVideo.smd
$(BIN)/SDLVideo.smd: $(BIN)/$(SRC)/lib_media/render/sdl_video.cpp.o
$(BIN)/SDLVideo.smd: LDFLAGS+=$(RENDER_LDFLAGS)
$(BIN)/$(SRC)/lib_media/render/sdl_video.cpp.o: CFLAGS+=$(RENDER_CFLAGS)

TARGETS+=$(BIN)/SDLAudio.smd
$(BIN)/SDLAudio.smd: \
	$(BIN)/$(SRC)/lib_media/render/sdl_audio.cpp.o \
	$(BIN)/$(SRC)/lib_media/common/libav.cpp.o
$(BIN)/SDLAudio.smd: LDFLAGS+=$(RENDER_LDFLAGS)
$(BIN)/$(SRC)/lib_media/render/sdl_audio.cpp.o: CFLAGS+=$(RENDER_CFLAGS)

