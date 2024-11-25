# Define the plugin library
add_library(TSDemuxer STATIC
    ts_demuxer.cpp
)

# Include directories if needed
target_include_directories(TSDemuxer PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

# Link libraries if required
# Example (if any external libraries are needed):
# target_link_libraries(TSDemuxer some_dependency)