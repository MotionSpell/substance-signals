MYDIR=$(call get-my-dir)

include $(SRC)/plugins/HlsDemuxer/project.mk
include $(SRC)/plugins/MulticastInput/project.mk
include $(SRC)/plugins/UdpOutput/project.mk
include $(SRC)/plugins/TsMuxer/project.mk
include $(SRC)/plugins/TsDemuxer/project.mk
include $(SRC)/plugins/Telx2Ttml/project.mk
include $(SRC)/plugins/Fmp4Splitter/project.mk

ifeq ($(SIGNALS_HAS_X11), 1)
include $(SRC)/plugins/SdlRender/render.mk
endif

