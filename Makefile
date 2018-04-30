CFLAGS:=$(CFLAGS)
CFLAGS+=-std=gnu++1y
CFLAGS+=-Wall
CFLAGS+=-Wextra
CFLAGS+=-fvisibility=hidden -fvisibility-inlines-hidden 
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
    LDFLAGS+=-s
  endif
endif

SIGNALS_HAS_X11?=1
SIGNALS_HAS_AWS?=0

ifeq ($(SIGNALS_HAS_X11), 1)
  CFLAGS+=-DSIGNALS_HAS_X11
endif

ifeq ($(SIGNALS_HAS_AWS), 1)
  CFLAGS+=-DSIGNALS_HAS_AWS
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

ifeq ($(SIGNALS_HAS_AWS), 1)
	/bin/echo 'LDFLAGS+=-laws-cpp-sdk-core -laws-cpp-sdk-mediastore -laws-cpp-sdk-mediastore-data' >> $(BIN)/config.mk.tmp ; 
endif

	/bin/echo 'CFLAGS+= -I$(EXTRA)/include/asio -Wno-unused-local-typedefs' >> $(BIN)/config.mk.tmp
	/bin/echo 'LDFLAGS+= -lpthread -lturbojpeg -lcurl' >> $(BIN)/config.mk.tmp ;
	mv $(BIN)/config.mk.tmp $(BIN)/config.mk

include $(BIN)/config.mk

CFLAGS+=-Umain

TARGETS:=
DEPS:=

define get-my-dir
$(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
endef

#------------------------------------------------------------------------------

UTILS_SRCS:=\
  src/version.cpp\
  $(SRC)/lib_utils/clock.cpp\
  $(SRC)/lib_utils/log.cpp\
  $(SRC)/lib_utils/scheduler.cpp\
  $(SRC)/lib_utils/time.cpp
LIB_UTILS_OBJS:=$(UTILS_SRCS:%.cpp=$(BIN)/%.o)

#------------------------------------------------------------------------------

MEDIA_SRCS:=\
  $(SRC)/lib_media/common/libav.cpp\
  $(SRC)/lib_media/common/gpac.cpp\
  $(SRC)/lib_media/common/picture.cpp\
  $(SRC)/lib_media/decode/jpegturbo_decode.cpp\
  $(SRC)/lib_media/decode/libav_decode.cpp\
  $(SRC)/lib_media/demux/gpac_demux_mp4_simple.cpp\
  $(SRC)/lib_media/demux/gpac_demux_mp4_full.cpp\
  $(SRC)/lib_media/demux/libav_demux.cpp\
  $(SRC)/lib_media/encode/jpegturbo_encode.cpp\
  $(SRC)/lib_media/encode/libav_encode.cpp\
  $(SRC)/lib_media/in/file.cpp\
  $(SRC)/lib_media/in/mpeg_dash_input.cpp\
  $(SRC)/lib_media/in/sound_generator.cpp\
  $(SRC)/lib_media/in/video_generator.cpp\
  $(SRC)/lib_media/mux/gpac_mux_m2ts.cpp\
  $(SRC)/lib_media/mux/gpac_mux_mp4.cpp\
  $(SRC)/lib_media/mux/gpac_mux_mp4_mss.cpp\
  $(SRC)/lib_media/mux/libav_mux.cpp\
  $(SRC)/lib_media/out/file.cpp\
  $(SRC)/lib_media/out/http.cpp\
  $(SRC)/lib_media/out/null.cpp\
  $(SRC)/lib_media/out/print.cpp\
  $(SRC)/lib_media/stream/apple_hls.cpp\
  $(SRC)/lib_media/stream/mpeg_dash.cpp\
  $(SRC)/lib_media/stream/ms_hss.cpp\
  $(SRC)/lib_media/stream/adaptive_streaming_common.cpp\
  $(SRC)/lib_media/transform/audio_convert.cpp\
  $(SRC)/lib_media/transform/audio_gap_filler.cpp\
  $(SRC)/lib_media/transform/libavfilter.cpp\
  $(SRC)/lib_media/transform/restamp.cpp\
  $(SRC)/lib_media/transform/telx2ttml.cpp\
  $(SRC)/lib_media/transform/time_rectifier.cpp\
  $(SRC)/lib_media/transform/video_convert.cpp\
  $(SRC)/lib_media/utils/comparator.cpp\
  $(SRC)/lib_media/utils/recorder.cpp\
  $(SRC)/lib_media/utils/repeater.cpp
ifeq ($(SIGNALS_HAS_X11), 1)
MEDIA_SRCS+=\
  $(SRC)/lib_media/render/sdl_audio.cpp\
  $(SRC)/lib_media/render/sdl_common.cpp\
  $(SRC)/lib_media/render/sdl_video.cpp
endif  
ifeq ($(SIGNALS_HAS_AWS), 1)
MEDIA_SRCS+=\
  $(SRC)/lib_media/out/aws_mediastore.cpp\
  $(SRC)/lib_media/out/aws_sdk_instance.cpp
endif  
LIB_MEDIA_OBJS:=$(MEDIA_SRCS:%.cpp=$(BIN)/%.o)

#------------------------------------------------------------------------------

PIPELINE_SRCS:=\
  $(SRC)//lib_pipeline/pipeline.cpp
LIB_PIPELINE_OBJS:=$(PIPELINE_SRCS:%.cpp=$(BIN)/%.o)

#------------------------------------------------------------------------------

MODULES_SRCS:=\
  $(SRC)/lib_modules/core/connection.cpp\
  $(SRC)/lib_modules/core/data.cpp
LIB_MODULES_OBJS:=$(MODULES_SRCS:%.cpp=$(BIN)/%.o)

#------------------------------------------------------------------------------

APPCOMMON_SRCS:=\
  $(SRC)/lib_appcommon/safemain.cpp
LIB_APPCOMMON_OBJS:=$(APPCOMMON_SRCS:%.cpp=$(BIN)/%.o)

#------------------------------------------------------------------------------

include $(SRC)/tests/project.mk

#------------------------------------------------------------------------------

include $(SRC)/apps/dashcastx/project.mk

#------------------------------------------------------------------------------

ifeq ($(SIGNALS_HAS_X11), 1)
include $(SRC)/apps/player/project.mk
endif

#------------------------------------------------------------------------------

include $(SRC)/apps/mp42tsx/project.mk

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
	$(CXX) "$<" -c $(CFLAGS) -o "$@" 
	@$(CXX) "$<" -c $(CFLAGS) -o "$(BIN)/$*.deps" -MP -MM -MT "$(BIN)/$*.o"

clean:
	rm -rf $(BIN)
	mkdir $(BIN)

#-------------------------------------------------------------------------------

-include $(shell test -d $(BIN) && find $(BIN) -name "*.deps")
