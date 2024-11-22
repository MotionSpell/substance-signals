# Define the plugin library
add_library(HlsDemuxer STATIC
    hls_demux.cpp
    hls_demux.hpp
)

# Link dependencies if any
target_link_libraries(HlsDemuxer ${CMAKE_THREAD_LIBS_INIT})

# Include directories
target_include_directories(HlsDemuxer PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
