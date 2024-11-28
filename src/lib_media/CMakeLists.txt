# Include source files
set(LIB_MEDIA_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/common/expand_vars.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/http_puller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/http_sender.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/iso8601.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/mpeg_dash_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/picture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/sax_xml_parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/xml.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/demux/dash_demux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/in/file.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/in/mpeg_dash_input.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/in/sound_generator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/in/video_generator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/out/file.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/out/http.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/out/http_sink.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/out/null.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/out/print.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/stream/apple_hls.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/stream/ms_hss.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/stream/adaptive_streaming_common.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transform/audio_gap_filler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transform/restamp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transform/rectifier.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/recorder.cpp
)


# Define the media library
add_library(media STATIC ${LIB_MEDIA_SRCS})
target_include_directories(media PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../  
)
target_link_libraries(media PRIVATE modules pipeline appcommon utils)


file(GLOB LIB_MODULES_SRCS 
    ${CMAKE_SOURCE_DIR}/src/lib_modules/utils/*.cpp
    ${CMAKE_SOURCE_DIR}/src/lib_modules/core/*.cpp
)

# Define source files for the AVCC2AnnexBConverter
set(EXE_AVCC2ANNEXBCONVERTER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/transform/avcc2annexb.cpp
)

# Combine all source files into a single list
list(APPEND EXE_AVCC2ANNEXBCONVERTER_SRCS
    ${LIB_MODULES_SRCS}
)

# Add the target (AVCC2AnnexBConverter.smd)
add_library(AVCC2AnnexBConverter SHARED ${EXE_AVCC2ANNEXBCONVERTER_SRCS})

# Include directories for the module
target_include_directories(AVCC2AnnexBConverter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/
)

target_link_libraries(AVCC2AnnexBConverter PRIVATE media modules pipeline appcommon utils)

set_target_properties(AVCC2AnnexBConverter PROPERTIES
    OUTPUT_NAME "AVCC2AnnexBConverter"         
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin  
    SUFFIX ".smd"  # Set the custom file extension to .smd
    PREFIX ""  
)



# Define source files for the LibavMuxHLSTS
set(EXE_LIBAVMUXHLSTS_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/stream/hls_muxer_libav.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/libav.cpp
)

# Combine all source files into a single list 
list(APPEND EXE_LIBAVMUXHLSTS_SRCS
    ${LIB_MODULES_SRCS}
)

# Add the target (LibavMuxHLSTS.smd)
add_library(LibavMuxHLSTS SHARED ${EXE_LIBAVMUXHLSTS_SRCS})

# Include directories for the module
target_include_directories(LibavMuxHLSTS PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/
)

# Link libraries to the target 
target_link_libraries(LibavMuxHLSTS PRIVATE media modules pipeline appcommon utils avutil avcodec)

# Set target properties
set_target_properties(LibavMuxHLSTS PROPERTIES
    OUTPUT_NAME "LibavMuxHLSTS"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    SUFFIX ".smd"  # Set the custom file extension to .smd
    PREFIX ""
)


# Define source files for the VideoConvert target
set(EXE_VIDEOCONVERTER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/transform/video_convert.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/libav.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/picture.cpp
)

list(APPEND EXE_VIDEOCONVERTER_SRCS
    ${LIB_MODULES_SRCS}
)

# Add the target (VideoConvert.smd)
add_library(VideoConvert SHARED ${EXE_VIDEOCONVERTER_SRCS})

# Include directories for the module
target_include_directories(VideoConvert PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/
)

# Link the necessary libraries
target_link_libraries(VideoConvert PRIVATE
    media modules pipeline appcommon utils
    avcodec avutil swscale
)

# Set properties for the target
set_target_properties(VideoConvert PROPERTIES
    OUTPUT_NAME "VideoConvert"  # Output name without extension
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    SUFFIX ".smd"  # Set the custom file extension to .smd
    PREFIX ""  # 
)




# add_executable(VideoConvert
#     "${CMAKE_CURRENT_SOURCE_DIR}/transform/video_convert.cpp"
#     "${CMAKE_CURRENT_SOURCE_DIR}/common/libav.cpp"
#     "${CMAKE_CURRENT_SOURCE_DIR}/common/picture.cpp"
# )
# target_link_libraries(VideoConvert PRIVATE media avutil avcodec swscale)

# add_executable(AudioConvert
#     "${CMAKE_CURRENT_SOURCE_DIR}/transform/audio_convert.cpp"
#     "${CMAKE_CURRENT_SOURCE_DIR}/common/libav.cpp"
# )
# target_link_libraries(AudioConvert PRIVATE media avutil avcodec swresample)

# add_executable(JPEGTurboDecode
#     "${CMAKE_CURRENT_SOURCE_DIR}/decode/jpegturbo_decode.cpp"
#     "${CMAKE_CURRENT_SOURCE_DIR}/common/picture.cpp"
# )
# target_link_libraries(JPEGTurboDecode PRIVATE media turbojpeg)

# add_executable(JPEGTurboEncode
#     "${CMAKE_CURRENT_SOURCE_DIR}/encode/jpegturbo_encode.cpp"
#     "${CMAKE_CURRENT_SOURCE_DIR}/common/picture.cpp"
# )
# target_link_libraries(JPEGTurboEncode PRIVATE media turbojpeg)

# add_executable(Encoder
#     "${CMAKE_CURRENT_SOURCE_DIR}/encode/libav_encode.cpp"
#     "${CMAKE_CURRENT_SOURCE_DIR}/common/libav_init.cpp"
#     "${CMAKE_CURRENT_SOURCE_DIR}/common/libav.cpp"
#     "${CMAKE_CURRENT_SOURCE_DIR}/common/picture.cpp"
# )
# target_link_libraries(Encoder PRIVATE media avcodec avutil)

# add_executable(Decoder
#     "${CMAKE_CURRENT_SOURCE_DIR}/decode/decoder.cpp"
#     "${CMAKE_CURRENT_SOURCE_DIR}/common/libav.cpp"
#     "${CMAKE_CURRENT_SOURCE_DIR}/common/picture.cpp"
# )
# target_link_libraries(Decoder PRIVATE media avcodec avutil)

# add_executable(LibavDemux
#     "${CMAKE_CURRENT_SOURCE_DIR}/common/libav_init.cpp"
#     "${CMAKE_CURRENT_SOURCE_DIR}/demux/libav_demux.cpp"
#     "${CMAKE_CURRENT_SOURCE_DIR}/common/libav.cpp"
#     "${CMAKE_CURRENT_SOURCE_DIR}/transform/restamp.cpp"
# )
# target_link_libraries(LibavDemux PRIVATE media avformat avcodec avutil avdevice)

# add_executable(LibavMux
#     "${CMAKE_CURRENT_SOURCE_DIR}/mux/libav_mux.cpp"
#     "${CMAKE_CURRENT_SOURCE_DIR}/common/libav_init.cpp"
#     "${CMAKE_CURRENT_SOURCE_DIR}/common/libav.cpp"
# )
# target_link_libraries(LibavMux PRIVATE media avformat avcodec avutil)

# add_executable(LibavFilter
#     "${CMAKE_CURRENT_SOURCE_DIR}/transform/libavfilter.cpp"
#     "${CMAKE_CURRENT_SOURCE_DIR}/common/picture.cpp"
#     "${CMAKE_CURRENT_SOURCE_DIR}/common/libav.cpp"
# )
# target_link_libraries(LibavFilter PRIVATE media avfilter avcodec avutil)

# add_executable(GPACMuxMP4 "${CMAKE_CURRENT_SOURCE_DIR}/mux/gpac_mux_mp4.cpp")
# target_link_libraries(GPACMuxMP4 PRIVATE media gpac)

# add_executable(GPACMuxMP4MSS
#     "${CMAKE_CURRENT_SOURCE_DIR}/mux/gpac_mux_mp4_mss.cpp"
#     "${CMAKE_CURRENT_SOURCE_DIR}/mux/gpac_mux_mp4.cpp"
# )
# target_link_libraries(GPACMuxMP4MSS PRIVATE media gpac)

# add_executable(GPACDemuxMP4Simple "${CMAKE_CURRENT_SOURCE_DIR}/demux/gpac_demux_mp4_simple.cpp")
# target_link_libraries(GPACDemuxMP4Simple PRIVATE media gpac)

# add_executable(GPACDemuxMP4Full "${CMAKE_CURRENT_SOURCE_DIR}/demux/gpac_demux_mp4_full.cpp")
# target_link_libraries(GPACDemuxMP4Full PRIVATE media gpac)

# add_executable(FileSystemSink "${CMAKE_CURRENT_SOURCE_DIR}/out/filesystem.cpp")
# target_link_libraries(FileSystemSink PRIVATE media)

# add_executable(LogoOverlay "${CMAKE_CURRENT_SOURCE_DIR}/transform/logo_overlay.cpp")
# target_link_libraries(LogoOverlay PRIVATE media)
