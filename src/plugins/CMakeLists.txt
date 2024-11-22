# Optionally enable X11 support
if(SIGNALS_HAS_X11)
    add_subdirectory(SdlRender)
endif()

# Include other plugins
add_subdirectory(HlsDemuxer)
add_subdirectory(MulticastInput)
add_subdirectory(UdpOutput)
add_subdirectory(TsMuxer)
add_subdirectory(TsDemuxer)
add_subdirectory(Telx2Ttml)
add_subdirectory(Fmp4Splitter)
add_subdirectory(Dasher)

