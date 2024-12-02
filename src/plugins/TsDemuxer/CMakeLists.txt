# Define the plugin library
add_library(TsDemuxer SHARED
    ts_demuxer.cpp
    )

# Link dependencies if any
target_link_libraries(TsDemuxer 
    ${CMAKE_THREAD_LIBS_INIT}
    modules
    )

# Include directories
target_include_directories(TsDemuxer PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

# Set properties for the target
set_target_properties(TsDemuxer PROPERTIES
    OUTPUT_NAME "TsDemuxer"  # Output name without extension
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    SUFFIX ".smd"  # Set the custom file extension to .smd
    PREFIX ""  # 
)
