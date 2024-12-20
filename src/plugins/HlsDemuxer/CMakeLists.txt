# Define source files for the HlsDemuxer target
set(EXE_HlsDemuxer_SRCS
    hls_demux.cpp
    ${SIGNALS_TOP_SOURCE_DIR}/src/lib_media/common/http_puller.cpp
    )
file(GLOB LIB_MODULES_SRCS 
    ${SIGNALS_TOP_SOURCE_DIR}/src/lib_modules/utils/*.cpp
    ${SIGNALS_TOP_SOURCE_DIR}/src/lib_modules/core/*.cpp
)


list(APPEND EXE_HlsDemuxer_SRCS
    ${LIB_MODULES_SRCS}
)

# Add the target (HlsDemuxer.smd)
add_library(HlsDemuxer SHARED ${EXE_HlsDemuxer_SRCS})

# Include directories for the module
target_include_directories(HlsDemuxer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SIGNALS_TOP_SOURCE_DIR}/src/
    )

# Link the necessary libraries
target_link_libraries(HlsDemuxer PRIVATE
    media modules pipeline appcommon utils CURL::libcurl
)

# Set properties for the target
set_target_properties(HlsDemuxer PROPERTIES
    OUTPUT_NAME "HlsDemuxer"  # Output name without extension
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    SUFFIX ".smd"  # Set the custom file extension to .smd
    PREFIX ""  # 
)

install(TARGETS HlsDemuxer
	EXPORT HlsDemuxer
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_custom_command(TARGET HlsDemuxer POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:HlsDemuxer> $<TARGET_RUNTIME_DLLS:HlsDemuxer>
  COMMAND_EXPAND_LISTS
)
