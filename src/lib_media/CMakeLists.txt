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

set_target_properties(media PROPERTIES
    OUTPUT_NAME signals_media
    POSITION_INDEPENDENT_CODE TRUE
    )

target_include_directories(media PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include/signals>
)

# Installation (optional) to be tested later
install(TARGETS media 
    EXPORT signals-media
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION include/signals
)

# Hmm. libmedia is a static library, so it can't be used for TARGET_RUNTIME_DLLs.
# Let's hope the smd's below will pick up the needed dependencies.
#add_custom_command(TARGET media POST_BUILD
#  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:media> $<TARGET_RUNTIME_DLLS:media>
#  COMMAND_EXPAND_LISTS
#)

export(
    EXPORT signals-media
    NAMESPACE signals::
)

install(EXPORT signals-media
    NAMESPACE signals::
    DESTINATION lib/cmake/signals
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DESTINATION include/signals/
    FILES_MATCHING
    PATTERN "*.hpp"
)
target_link_libraries(media 
    PUBLIC 
    modules 
    pipeline 
    appcommon 
    utils
    gpac::gpac 
    CURL::libcurl
    )

# xxxjack I think this should go... Won't we link to some objects twice?
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

target_link_libraries(AVCC2AnnexBConverter 
    PRIVATE 
    media 
    modules 
    pipeline 
    appcommon 
    utils
    )

set_target_properties(AVCC2AnnexBConverter PROPERTIES
    OUTPUT_NAME "AVCC2AnnexBConverter"         
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin  
    SUFFIX ".smd"  # Set the custom file extension to .smd
    PREFIX ""  
)


install(TARGETS AVCC2AnnexBConverter
	EXPORT AVCC2AnnexBConverter
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_custom_command(TARGET AVCC2AnnexBConverter POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:AVCC2AnnexBConverter> $<TARGET_RUNTIME_DLLS:AVCC2AnnexBConverter>
  COMMAND_EXPAND_LISTS
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
    ${FFMPEG_INCLUDE_DIRS}
)

# Link libraries to the target 
target_link_libraries(LibavMuxHLSTS 
    PRIVATE 
    media 
    modules 
    pipeline 
    appcommon 
    ${FFMPEG_LIBRARIES}
    )

# Set target properties
set_target_properties(LibavMuxHLSTS PROPERTIES
    OUTPUT_NAME "LibavMuxHLSTS"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    SUFFIX ".smd"  # Set the custom file extension to .smd
    PREFIX ""
)

install(TARGETS LibavMuxHLSTS
	EXPORT LibavMuxHLSTS
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_custom_command(TARGET LibavMuxHLSTS POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:LibavMuxHLSTS> $<TARGET_RUNTIME_DLLS:LibavMuxHLSTS>
  COMMAND_EXPAND_LISTS
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
    ${FFMPEG_INCLUDE_DIRS}
)

# Link the necessary libraries
target_link_libraries(VideoConvert PRIVATE
    media modules pipeline appcommon utils
    ${FFMPEG_LIBRARIES}
)

# Set properties for the target
set_target_properties(VideoConvert PROPERTIES
    OUTPUT_NAME "VideoConvert"  # Output name without extension
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    SUFFIX ".smd"  # Set the custom file extension to .smd
    PREFIX ""  # 
)

install(TARGETS VideoConvert
	EXPORT VideoConvert
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_custom_command(TARGET VideoConvert POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:VideoConvert> $<TARGET_RUNTIME_DLLS:VideoConvert>
  COMMAND_EXPAND_LISTS
)

# AudioConvert target
set(EXE_AUDIOCONVERTER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/transform/audio_convert.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/libav.cpp
)
list(APPEND EXE_AUDIOCONVERTER_SRCS ${LIB_MODULES_SRCS})
add_library(AudioConvert SHARED ${EXE_AUDIOCONVERTER_SRCS})
target_include_directories(AudioConvert 
    PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR} 
    ${CMAKE_SOURCE_DIR}/src/
    ${FFMPEG_INCLUDE_DIRS}
    )
target_link_libraries(AudioConvert 
    PRIVATE 
    media 
    modules 
    pipeline 
    appcommon 
    utils 
    ${FFMPEG_LIBRARIES}
    )
set_target_properties(AudioConvert PROPERTIES 
    OUTPUT_NAME "AudioConvert" 
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin 
    SUFFIX ".smd" 
    PREFIX ""
    )

install(TARGETS AudioConvert
	EXPORT AudioConvert
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_custom_command(TARGET AudioConvert POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:AudioConvert> $<TARGET_RUNTIME_DLLS:AudioConvert>
  COMMAND_EXPAND_LISTS
)

# JPEGTurboDecode target
set(EXE_JPEGTURBODECODE_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/decode/jpegturbo_decode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/picture.cpp
)
list(APPEND EXE_JPEGTURBODECODE_SRCS ${LIB_MODULES_SRCS})
add_library(JPEGTurboDecode SHARED ${EXE_JPEGTURBODECODE_SRCS})
target_include_directories(JPEGTurboDecode 
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} 
    ${CMAKE_SOURCE_DIR}/src/
    )
target_link_libraries(JPEGTurboDecode 
    PRIVATE 
    media 
    modules 
    pipeline 
    appcommon 
    utils 
    libjpeg-turbo::turbojpeg
    )
set_target_properties(JPEGTurboDecode PROPERTIES 
    OUTPUT_NAME "JPEGTurboDecode" 
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin 
    SUFFIX ".smd" 
    PREFIX ""
    )

install(TARGETS JPEGTurboDecode
	EXPORT JPEGTurboDecode
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_custom_command(TARGET JPEGTurboDecode POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:JPEGTurboDecode> $<TARGET_RUNTIME_DLLS:JPEGTurboDecode>
  COMMAND_EXPAND_LISTS
)

# JPEGTurboEncode target
set(EXE_JPEGTURBOENCODE_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/encode/jpegturbo_encode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/picture.cpp
)
list(APPEND EXE_JPEGTURBOENCODE_SRCS ${LIB_MODULES_SRCS})
add_library(JPEGTurboEncode SHARED ${EXE_JPEGTURBOENCODE_SRCS})
target_include_directories(JPEGTurboEncode PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src/)
target_link_libraries(JPEGTurboEncode 
    PRIVATE 
    media 
    modules 
    pipeline 
    appcommon 
    utils 
    libjpeg-turbo::turbojpeg
    )
set_target_properties(JPEGTurboEncode PROPERTIES 
    OUTPUT_NAME "JPEGTurboEncode" 
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin 
    SUFFIX ".smd" 
    PREFIX ""
    )


install(TARGETS JPEGTurboEncode
	EXPORT JPEGTurboEncode
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_custom_command(TARGET JPEGTurboEncode POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:JPEGTurboEncode> $<TARGET_RUNTIME_DLLS:JPEGTurboEncode>
  COMMAND_EXPAND_LISTS
)

# Encoder target
set(EXE_ENCODER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/encode/libav_encode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/libav_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/libav.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/picture.cpp
)
list(APPEND EXE_ENCODER_SRCS ${LIB_MODULES_SRCS})
add_library(Encoder SHARED ${EXE_ENCODER_SRCS})
target_include_directories(Encoder 
    PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR} 
    ${CMAKE_SOURCE_DIR}/src/
    ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(Encoder 
    PRIVATE 
    media 
    modules 
    pipeline 
    appcommon 
    utils 
    ${FFMPEG_LIBRARIES}
    )
set_target_properties(Encoder PROPERTIES 
    OUTPUT_NAME "Encoder" 
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin 
    SUFFIX ".smd" 
    PREFIX ""
    )

install(TARGETS Encoder
	EXPORT Encoder
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_custom_command(TARGET Encoder POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:Encoder> $<TARGET_RUNTIME_DLLS:Encoder>
  COMMAND_EXPAND_LISTS
)

# Decoder target
set(EXE_DECODER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/decode/decoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/libav.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/picture.cpp
)
list(APPEND EXE_DECODER_SRCS ${LIB_MODULES_SRCS})
add_library(Decoder SHARED ${EXE_DECODER_SRCS})
target_include_directories(Decoder 
    PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR} 
    ${CMAKE_SOURCE_DIR}/src/
    ${FFMPEG_INCLUDE_DIRS}
    )
target_link_libraries(Decoder 
    PRIVATE 
    media 
    modules 
    pipeline 
    appcommon 
    utils 
    ${FFMPEG_LIBRARIES}
    )
set_target_properties(Decoder PROPERTIES 
    OUTPUT_NAME "Decoder" 
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin 
    SUFFIX ".smd" 
    PREFIX ""
    )

install(TARGETS Decoder
	EXPORT Decoder
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_custom_command(TARGET Decoder POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:Decoder> $<TARGET_RUNTIME_DLLS:Decoder>
  COMMAND_EXPAND_LISTS
)

# LibavDemux target
set(EXE_LIBAVDEMUX_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/common/libav_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/demux/libav_demux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/libav.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transform/restamp.cpp
)
list(APPEND EXE_LIBAVDEMUX_SRCS ${LIB_MODULES_SRCS})
add_library(LibavDemux SHARED ${EXE_LIBAVDEMUX_SRCS})
target_include_directories(LibavDemux 
    PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR} 
    ${CMAKE_SOURCE_DIR}/src/
    ${FFMPEG_INCLUDE_DIRS}
    )
target_link_libraries(LibavDemux 
    PRIVATE 
    media 
    modules 
    pipeline 
    appcommon 
    utils 
    ${FFMPEG_LIBRARIES}
    )
set_target_properties(LibavDemux 
    PROPERTIES 
    OUTPUT_NAME "LibavDemux" 
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin 
    SUFFIX ".smd" 
    PREFIX ""
    )

install(TARGETS LibavDemux
	EXPORT LibavDemux
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_custom_command(TARGET LibavDemux POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:LibavDemux> $<TARGET_RUNTIME_DLLS:LibavDemux>
  COMMAND_EXPAND_LISTS
)

# LibavMux target
set(EXE_LIBAVMUX_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/mux/libav_mux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/libav_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/libav.cpp
)
list(APPEND EXE_LIBAVMUX_SRCS ${LIB_MODULES_SRCS})
add_library(LibavMux SHARED ${EXE_LIBAVMUX_SRCS})
target_include_directories(LibavMux 
    PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR} 
    ${CMAKE_SOURCE_DIR}/src/
    ${FFMPEG_INCLUDE_DIRS}
    )
target_link_libraries(LibavMux 
    PRIVATE 
    media 
    modules 
    pipeline 
    appcommon 
    utils 
    ${FFMPEG_LIBRARIES}
    )
set_target_properties(LibavMux PROPERTIES 
    OUTPUT_NAME "LibavMux" 
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin 
    SUFFIX ".smd" 
    PREFIX ""
    )


install(TARGETS LibavMux
	EXPORT LibavMux
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_custom_command(TARGET LibavMux POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:LibavMux> $<TARGET_RUNTIME_DLLS:LibavMux>
  COMMAND_EXPAND_LISTS
)

# LibavFilter target
set(EXE_LIBAVFILTER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/transform/libavfilter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/picture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/libav.cpp
)
list(APPEND EXE_LIBAVFILTER_SRCS ${LIB_MODULES_SRCS})
add_library(LibavFilter SHARED ${EXE_LIBAVFILTER_SRCS})
target_include_directories(LibavFilter 
    PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR} 
    ${CMAKE_SOURCE_DIR}/src/
    ${FFMPEG_INCLUDE_DIRS}
    )
target_link_libraries(LibavFilter 
    PRIVATE 
    media 
    modules 
    pipeline 
    appcommon 
    utils 
    ${FFMPEG_LIBRARIES}
)
set_target_properties(LibavFilter PROPERTIES 
    OUTPUT_NAME "LibavFilter" 
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin 
    SUFFIX ".smd" 
    PREFIX ""
    )

install(TARGETS LibavFilter
	EXPORT LibavFilter
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_custom_command(TARGET LibavFilter POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:LibavFilter> $<TARGET_RUNTIME_DLLS:LibavFilter>
  COMMAND_EXPAND_LISTS
)

# GPACMuxMP4 target
set(EXE_GPACMUXMP4_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/mux/gpac_mux_mp4.cpp
)
list(APPEND EXE_GPACMUXMP4_SRCS ${LIB_MODULES_SRCS})
add_library(GPACMuxMP4 SHARED ${EXE_GPACMUXMP4_SRCS})
target_include_directories(GPACMuxMP4 
    PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR} 
    ${CMAKE_SOURCE_DIR}/src/
    ${FFMPEG_INCLUDE_DIRS}
    )
target_link_libraries(GPACMuxMP4 
    PRIVATE 
    media 
    modules 
    pipeline 
    appcommon 
    utils 
    gpac::gpac
    ${FFMPEG_LIBRARIES}
    )
set_target_properties(GPACMuxMP4 PROPERTIES 
    OUTPUT_NAME "GPACMuxMP4" 
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin 
    SUFFIX ".smd" 
    PREFIX ""
    )

install(TARGETS GPACMuxMP4
	EXPORT GPACMuxMP4
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_custom_command(TARGET GPACMuxMP4 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:GPACMuxMP4> $<TARGET_RUNTIME_DLLS:GPACMuxMP4>
  COMMAND_EXPAND_LISTS
)

# GPACMuxMP4MSS target
set(EXE_GPACMUXMP4MSS_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/mux/gpac_mux_mp4_mss.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mux/gpac_mux_mp4.cpp
)
list(APPEND EXE_GPACMUXMP4MSS_SRCS ${LIB_MODULES_SRCS})
add_library(GPACMuxMP4MSS SHARED ${EXE_GPACMUXMP4MSS_SRCS})
target_include_directories(GPACMuxMP4MSS 
    PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR} 
    ${CMAKE_SOURCE_DIR}/src/
    )
target_link_libraries(GPACMuxMP4MSS 
    PRIVATE 
    media 
    modules 
    pipeline 
    appcommon 
    utils 
    gpac::gpac
    )
set_target_properties(GPACMuxMP4MSS PROPERTIES 
    OUTPUT_NAME "GPACMuxMP4MSS" 
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin 
    SUFFIX ".smd" 
    PREFIX ""
    )

install(TARGETS GPACMuxMP4MSS
	EXPORT GPACMuxMP4MSS
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_custom_command(TARGET GPACMuxMP4MSS POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:GPACMuxMP4MSS> $<TARGET_RUNTIME_DLLS:GPACMuxMP4MSS>
  COMMAND_EXPAND_LISTS
)


# GPACDemuxMP4Simple target
set(EXE_GPACDEMUXMP4SIMPLE_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/demux/gpac_demux_mp4_simple.cpp
)
list(APPEND EXE_GPACDEMUXMP4SIMPLE_SRCS ${LIB_MODULES_SRCS})
add_library(GPACDemuxMP4Simple SHARED ${EXE_GPACDEMUXMP4SIMPLE_SRCS})
target_include_directories(GPACDemuxMP4Simple 
    PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR} 
    ${CMAKE_SOURCE_DIR}/src/
    )
target_link_libraries(GPACDemuxMP4Simple 
    PRIVATE 
    media 
    modules 
    pipeline 
    appcommon 
    utils 
    gpac::gpac
    )
set_target_properties(GPACDemuxMP4Simple PROPERTIES 
    OUTPUT_NAME "GPACDemuxMP4Simple" 
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin 
    SUFFIX ".smd" 
    PREFIX ""
    )


install(TARGETS GPACDemuxMP4Simple
	EXPORT GPACDemuxMP4Simple
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_custom_command(TARGET GPACDemuxMP4Simple POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:GPACDemuxMP4Simple> $<TARGET_RUNTIME_DLLS:GPACDemuxMP4Simple>
  COMMAND_EXPAND_LISTS
)

# GPACDemuxMP4Full target
set(EXE_GPACDEMUXMP4FULL_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/demux/gpac_demux_mp4_full.cpp
)
list(APPEND EXE_GPACDEMUXMP4FULL_SRCS ${LIB_MODULES_SRCS})
add_library(GPACDemuxMP4Full SHARED ${EXE_GPACDEMUXMP4FULL_SRCS})
target_include_directories(GPACDemuxMP4Full 
    PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR} 
    ${CMAKE_SOURCE_DIR}/src/
    )
target_link_libraries(GPACDemuxMP4Full 
    PRIVATE 
    media 
    modules 
    pipeline 
    appcommon 
    utils 
    gpac::gpac
    ${FFMPEG_LIBRARIES}
    )
set_target_properties(GPACDemuxMP4Full PROPERTIES 
    OUTPUT_NAME "GPACDemuxMP4Full" 
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin 
    SUFFIX ".smd" 
    PREFIX ""
    )

install(TARGETS GPACDemuxMP4Full
	EXPORT GPACDemuxMP4Full
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_custom_command(TARGET GPACDemuxMP4Full POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:GPACDemuxMP4Full> $<TARGET_RUNTIME_DLLS:GPACDemuxMP4Full>
  COMMAND_EXPAND_LISTS
)

# FileSystemSink target
set(EXE_FILESYSTEMSINK_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/out/filesystem.cpp
)
list(APPEND EXE_FILESYSTEMSINK_SRCS ${LIB_MODULES_SRCS})
add_library(FileSystemSink SHARED ${EXE_FILESYSTEMSINK_SRCS})
target_include_directories(FileSystemSink 
    PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR} 
    ${CMAKE_SOURCE_DIR}/src/
    )
target_link_libraries(FileSystemSink 
    PRIVATE 
    media 
    modules 
    pipeline 
    appcommon 
    utils
    )
set_target_properties(FileSystemSink PROPERTIES 
    OUTPUT_NAME "FileSystemSink" 
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin 
    SUFFIX ".smd" 
    PREFIX ""
    )

install(TARGETS FileSystemSink
	EXPORT FileSystemSink
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_custom_command(TARGET FileSystemSink POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:FileSystemSink> $<TARGET_RUNTIME_DLLS:FileSystemSink>
  COMMAND_EXPAND_LISTS
)

# LogoOverlay target
set(EXE_LOGOOVERLAY_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/transform/logo_overlay.cpp
)
list(APPEND EXE_LOGOOVERLAY_SRCS ${LIB_MODULES_SRCS})
add_library(LogoOverlay SHARED ${EXE_LOGOOVERLAY_SRCS})
target_include_directories(LogoOverlay 
    PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR} 
    ${CMAKE_SOURCE_DIR}/src/
    )
target_link_libraries(LogoOverlay 
    PRIVATE 
    media 
    modules 
    pipeline 
    appcommon 
    utils
    )
    
set_target_properties(LogoOverlay PROPERTIES 
    OUTPUT_NAME "LogoOverlay" 
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin 
    SUFFIX ".smd" 
    PREFIX ""
    )


install(TARGETS LogoOverlay
	EXPORT LogoOverlay
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_custom_command(TARGET LogoOverlay POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:LogoOverlay> $<TARGET_RUNTIME_DLLS:LogoOverlay>
  COMMAND_EXPAND_LISTS
)