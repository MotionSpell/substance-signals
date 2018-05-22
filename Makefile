.DELETE_ON_ERROR:

CFLAGS:=$(CFLAGS)
CFLAGS+=-std=gnu++1y
CFLAGS+=-Wall
CFLAGS+=-Wextra
CFLAGS+=-Werror
CFLAGS+=-fvisibility=hidden -fvisibility-inlines-hidden 
CFLAGS+=-W
CFLAGS+=-D__STDC_CONSTANT_MACROS

BIN?=bin
SRC?=src
EXTRA?=$(CURDIR)/sysroot

# always optimize
CFLAGS+=-O3

# default to debug mode
DEBUG?=1
ifeq ($(DEBUG), 1)
  CFLAGS+=-Wno-deprecated-declarations
  CFLAGS+=-g3
  LDFLAGS+=-g
else
  # disable all warnings in release mode:
  # the code must always build, especially old versions with recent compilers
  CFLAGS+=-w -DNDEBUG
  LDFLAGS+=-Xlinker -s
endif

SIGNALS_HAS_X11?=1

CFLAGS+=-I$(SRC) -I$(SRC)/lib_modules

CFLAGS+=-I$(EXTRA)/include
LDFLAGS+=-L$(EXTRA)/lib

LDFLAGS+=$(LDLIBS)

all: targets

PKGS:=\
  libavcodec\
  libavdevice\
  libavfilter\
  libavformat\
  libavutil\
  libswresample\
  libswscale\
  x264\
  freetype2\
  gpac\
  libcurl\
  libturbojpeg\

# Workaround: linking with libgpac and libavfilter currently requires this,
# otherwise link fails with undefined references to libass, libjpeg and libpng.
# (extra.sh builds libgpac/libavfilter with 'rpath' options, this might be root cause).
LDFLAGS+=-Wl,-rpath,$(EXTRA)/lib

$(BIN)/config.mk:
	@echo "Configuring ..."
	@set -e ; \
	mkdir -p $(BIN) ; \
	export PKG_CONFIG_PATH=$(EXTRA)/lib/pkgconfig:$$PKG_CONFIG_PATH ; \
	echo $(EXTRA); \
	/bin/echo '# config file' > $(BIN)/config.mk.tmp ; \
	/bin/echo -n 'CFLAGS+=' >> $(BIN)/config.mk.tmp ; \
	pkg-config --cflags $(PKGS) >> $(BIN)/config.mk.tmp ; \
	/bin/echo -n 'LDFLAGS+=' >> $(BIN)/config.mk.tmp ; \
	pkg-config --libs $(PKGS) >> $(BIN)/config.mk.tmp
	/bin/echo 'CFLAGS+= -I$(EXTRA)/include/asio -Wno-unused-local-typedefs' >> $(BIN)/config.mk.tmp
	/bin/echo 'LDFLAGS+= -lpthread' >> $(BIN)/config.mk.tmp ;
	mv $(BIN)/config.mk.tmp $(BIN)/config.mk

include $(BIN)/config.mk

CFLAGS+=-Umain

TARGETS:=
DEPS:=

define get-my-dir
$(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
endef

include $(SRC)/lib_utils/project.mk
include $(SRC)/lib_media/project.mk

#------------------------------------------------------------------------------

LIB_PIPELINE_SRCS:=\
  $(SRC)/lib_pipeline/pipeline.cpp

#------------------------------------------------------------------------------

LIB_MODULES_SRCS:=\
  $(SRC)/lib_modules/core/connection.cpp\
  $(SRC)/lib_modules/core/data.cpp

#------------------------------------------------------------------------------

LIB_APPCOMMON_SRCS:=\
  $(SRC)/lib_appcommon/safemain.cpp

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

VER_NEW:=$(TAG)-$(BRANCH)-rev$(VERSION)

$(BIN)/src/version.cpp.o: CFLAGS+=-DVERSION="\"$(VER_NEW)\""

targets: $(TARGETS)

#------------------------------------------------------------------------------

$(BIN)/%.exe:
	@mkdir -p $(dir $@)
	$(CXX) -o "$@" $^ $(LDFLAGS)

$(BIN)/%.cpp.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) "$<" -c $(CFLAGS) -o "$@" 
	@$(CXX) "$<" -c $(CFLAGS) -o "$@.deps" -MP -MM -MT "$@"

clean:
	rm -rf $(BIN)
	mkdir $(BIN)

#-------------------------------------------------------------------------------

-include $(shell test -d $(BIN) && find $(BIN) -name "*.deps")
