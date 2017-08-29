CFLAGS:=$(CFLAGS)
CFLAGS+=-std=gnu++1y
CFLAGS+=-Wall
CFLAGS+=-Wextra
CFLAGS+=-fvisibility=hidden
CFLAGS+=-W
CFLAGS+=-Wno-unused-parameter
CFLAGS+=-Wno-unused-function
CFLAGS+=-Wno-unused-label
CFLAGS+=-Wno-write-strings

CFLAGS+=-D__STDC_CONSTANT_MACROS

BIN?=bin
SRC?=src
EXTRA?=$(CURDIR)/extra

# default to debug mode
DEBUG?=1
COMPILER:=$(shell $(CXX) -v 2>&1 | grep -q -e "LLVM version" -e "clang version" && echo clang || echo gcc)
ifeq ($(DEBUG), 1)
  CFLAGS+=-Werror -Wno-deprecated-declarations
  CFLAGS+=-g3
  LDFLAGS+=-g
else
  CFLAGS+=-Werror -O3 -DNDEBUG -Wno-unused-variable -Wno-deprecated-declarations
  ifneq ($(COMPILER), clang)
    CFLAGS+=-s
  endif
  LDFLAGS+=-s
endif

#default has X11
SIGNALS_HAS_X11?=1

ifeq ($(SIGNALS_HAS_X11), 1)
  CFLAGS+=-DSIGNALS_HAS_X11
endif

CFLAGS+=-I$(SRC) -I$(SRC)/lib_modules

CFLAGS+=-I$(EXTRA)/include
LDFLAGS+=-L$(EXTRA)/lib

LDFLAGS+=$(LDLIBS)

all: targets

$(BIN)/config.mk:
	@echo "Configuring ..."
	@set -e ; \
	mkdir -p $(BIN) ; \
	export PKG_CONFIG_PATH=$(EXTRA)/lib/pkgconfig:$$PKG_CONFIG_PATH ; \
	echo $(EXTRA); \
	/bin/echo '# config file' > $(BIN)/config.mk.tmp ; \
	/bin/echo -n 'CFLAGS+=' >> $(BIN)/config.mk.tmp ; \
	pkg-config --cflags libavcodec libavdevice libavfilter libavformat libavutil libswresample libswscale x264 freetype2 >> $(BIN)/config.mk.tmp ; \
	/bin/echo -n 'LDFLAGS+=' >> $(BIN)/config.mk.tmp ; \
	pkg-config --libs libavcodec libavdevice libavfilter libavformat libavutil libswresample libswscale x264 freetype2 gpac >> $(BIN)/config.mk.tmp

ifeq ($(SIGNALS_HAS_X11), 1)
	export PKG_CONFIG_PATH=$(EXTRA)/lib/pkgconfig:$$PKG_CONFIG_PATH ; \
	/bin/echo -n 'CFLAGS+=' >> $(BIN)/config.mk.tmp ; \
	pkg-config --cflags sdl2 >> $(BIN)/config.mk.tmp ; \
	/bin/echo -n 'LDFLAGS+=' >> $(BIN)/config.mk.tmp ; \
	pkg-config --libs sdl2 >> $(BIN)/config.mk.tmp
endif

	/bin/echo 'CFLAGS+= -Wno-unused-local-typedefs' >> $(BIN)/config.mk.tmp
	/bin/echo 'LDFLAGS+= -lpthread -lturbojpeg -lcurl' >> $(BIN)/config.mk.tmp ;
	mv $(BIN)/config.mk.tmp $(BIN)/config.mk

include $(BIN)/config.mk

CFLAGS+=-Umain

TARGETS:=
DEPS:=

#------------------------------------------------------------------------------

ProjectName:=$(SRC)/lib_utils
UTILS_SRCS:=\
  src/version.cpp\
  $(ProjectName)/clock.cpp\
  $(ProjectName)/log.cpp\
  $(ProjectName)/scheduler.cpp\
  $(ProjectName)/time.cpp
LIB_UTILS_OBJS:=$(UTILS_SRCS:%.cpp=$(BIN)/%.o)

#------------------------------------------------------------------------------

ProjectName:=$(SRC)/lib_media
MEDIA_SRCS:=\
  $(ProjectName)/common/libav.cpp\
  $(ProjectName)/common/gpac.cpp\
  $(ProjectName)/common/picture.cpp\
  $(ProjectName)/decode/jpegturbo_decode.cpp\
  $(ProjectName)/decode/libav_decode.cpp\
  $(ProjectName)/demux/gpac_demux_mp4_simple.cpp\
  $(ProjectName)/demux/gpac_demux_mp4_full.cpp\
  $(ProjectName)/demux/libav_demux.cpp\
  $(ProjectName)/encode/jpegturbo_encode.cpp\
  $(ProjectName)/encode/libav_encode.cpp\
  $(ProjectName)/in/file.cpp\
  $(ProjectName)/in/mpeg_dash_input.cpp\
  $(ProjectName)/in/sound_generator.cpp\
  $(ProjectName)/in/video_generator.cpp\
  $(ProjectName)/mux/gpac_mux_m2ts.cpp\
  $(ProjectName)/mux/gpac_mux_mp4.cpp\
  $(ProjectName)/mux/gpac_mux_mp4_mss.cpp\
  $(ProjectName)/mux/libav_mux.cpp\
  $(ProjectName)/out/file.cpp\
  $(ProjectName)/out/http.cpp\
  $(ProjectName)/out/null.cpp\
  $(ProjectName)/out/print.cpp\
  $(ProjectName)/stream/apple_hls.cpp\
  $(ProjectName)/stream/mpeg_dash.cpp\
  $(ProjectName)/stream/ms_hss.cpp\
  $(ProjectName)/stream/adaptive_streaming_common.cpp\
  $(ProjectName)/transform/audio_convert.cpp\
  $(ProjectName)/transform/libavfilter.cpp\
  $(ProjectName)/transform/restamp.cpp\
  $(ProjectName)/transform/telx2ttml.cpp\
  $(ProjectName)/transform/time_rectifier.cpp\
  $(ProjectName)/transform/video_convert.cpp\
  $(ProjectName)/utils/comparator.cpp\
  $(ProjectName)/utils/recorder.cpp\
  $(ProjectName)/utils/repeater.cpp
ifeq ($(SIGNALS_HAS_X11), 1)
MEDIA_SRCS+=\
  $(ProjectName)/render/sdl_audio.cpp\
  $(ProjectName)/render/sdl_common.cpp\
  $(ProjectName)/render/sdl_video.cpp
endif  
LIB_MEDIA_OBJS:=$(MEDIA_SRCS:%.cpp=$(BIN)/%.o)
DEPS+=$(LIB_MEDIA_OBJS:%.o=%.deps)

#------------------------------------------------------------------------------

ProjectName:=$(SRC)/lib_pipeline
PIPELINE_SRCS:=\
  $(ProjectName)/pipeline.cpp
LIB_PIPELINE_OBJS:=$(PIPELINE_SRCS:%.cpp=$(BIN)/%.o)
DEPS+=$(LIB_PIPELINE_OBJS:%.o=%.deps)

#------------------------------------------------------------------------------

ProjectName:=$(SRC)/lib_modules
MODULES_SRCS:=\
  $(ProjectName)/core/connexion.cpp\
  $(ProjectName)/core/data.cpp
LIB_MODULES_OBJS:=$(MODULES_SRCS:%.cpp=$(BIN)/%.o)
DEPS+=$(LIB_MODULES_OBJS:%.o=%.deps)

#------------------------------------------------------------------------------

ProjectName:=$(SRC)/lib_appcommon
APPCOMMON_SRCS:=\
  $(ProjectName)/safemain.cpp
LIB_APPCOMMON_OBJS:=$(APPCOMMON_SRCS:%.cpp=$(BIN)/%.o)
DEPS+=$(LIB_APPCOMMON_OBJS:%.o=%.deps)

#------------------------------------------------------------------------------

ProjectName:=$(SRC)/tests
include $(ProjectName)/project.mk

#------------------------------------------------------------------------------

ProjectName:=$(SRC)/apps/dashcastx
include $(ProjectName)/project.mk

#------------------------------------------------------------------------------

ifeq ($(SIGNALS_HAS_X11), 1)

ProjectName:=$(SRC)/apps/player
include $(ProjectName)/project.mk

endif

#------------------------------------------------------------------------------

ProjectName:=$(SRC)/apps/mp42tsx
include $(ProjectName)/project.mk

#------------------------------------------------------------------------------

TAG:=$(shell echo `git describe --tags --abbrev=0 2> /dev/null || echo "UNKNOWN"`)
VERSION:=$(shell echo `git describe --tags --long 2> /dev/null || echo "UNKNOWN"` | sed "s/^$(TAG)-//")
BRANCH:=$(shell git rev-parse --abbrev-ref HEAD 2> /dev/null || echo "UNKNOWN")

VER_CUR:=$(shell echo `cat src/version.cpp`)
VER_NEW:=$(shell echo "const char *g_version = \"$(TAG)-$(BRANCH)-rev$(VERSION)\";")

version:
	@if [ '$(VER_CUR)' != '$(VER_NEW)' ] ; then \
		echo '$(VER_NEW)' > src/version.cpp; \
	fi

targets: version $(TARGETS)

#------------------------------------------------------------------------------

$(BIN)/%.exe:
	@mkdir -p $(dir $@)
	$(CXX) -o "$@" $^ $(LDFLAGS)

$(BIN)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) "$<" -c $(CFLAGS) -o "$(BIN)/$*.deps" -MM -MT "$(BIN)/$*.o"
	$(CXX) "$<" -c $(CFLAGS) -o "$@" 

clean:
	rm -rf $(BIN)
	mkdir $(BIN)

#-------------------------------------------------------------------------------

-include $(DEPS)
