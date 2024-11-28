# Define source files for the HlsDemuxer target
set(EXE_HlsDemuxer_SRCS
    hls_demux.cpp
    ${CMAKE_SOURCE_DIR}/src/lib_media/common/http_puller.cpp
    )
file(GLOB LIB_MODULES_SRCS 
    ${CMAKE_SOURCE_DIR}/src/lib_modules/utils/*.cpp
    ${CMAKE_SOURCE_DIR}/src/lib_modules/core/*.cpp
)


list(APPEND EXE_HlsDemuxer_SRCS
    ${LIB_MODULES_SRCS}
)

# Add the target (HlsDemuxer.smd)
add_library(HlsDemuxer SHARED ${EXE_HlsDemuxer_SRCS})

# Include directories for the module
target_include_directories(HlsDemuxer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/
    )

# Link the necessary libraries
target_link_libraries(HlsDemuxer PRIVATE
    media modules pipeline appcommon utils curl
)

# Set properties for the target
set_target_properties(HlsDemuxer PROPERTIES
    OUTPUT_NAME "HlsDemuxer"  # Output name without extension
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    SUFFIX ".smd"  # Set the custom file extension to .smd
    PREFIX ""  # 
)
