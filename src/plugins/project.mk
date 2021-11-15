MYDIR=$(call get-my-dir)

include $(SRC)/plugins/Dasher/project.mk
include $(SRC)/plugins/Fmp4Splitter/project.mk
include $(SRC)/plugins/GpacFilters/project.mk
include $(SRC)/plugins/HlsDemuxer/project.mk
include $(SRC)/plugins/HttpInput/project.mk
include $(SRC)/plugins/SocketInput/project.mk
include $(SRC)/plugins/RegulatorMono/project.mk
include $(SRC)/plugins/RegulatorMulti/project.mk
include $(SRC)/plugins/TsMuxer/project.mk
include $(SRC)/plugins/TsDemuxer/project.mk
include $(SRC)/plugins/TelxDecoder/project.mk
include $(SRC)/plugins/TtmlEncoder/project.mk
include $(SRC)/plugins/UdpOutput/project.mk

ifeq ($(SIGNALS_HAS_X11), 1)
include $(SRC)/plugins/SdlRender/render.mk
endif

