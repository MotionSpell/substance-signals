# Define the plugin library
add_library(FMP4SPLITTER SHARED
    fmp4_splitter.cpp
)

# Link dependencies if any
target_link_libraries(FMP4SPLITTER ${CMAKE_THREAD_LIBS_INIT})

# Include directories
target_include_directories(FMP4SPLITTER PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

# Set properties for the target
set_target_properties(FMP4SPLITTER PROPERTIES
    OUTPUT_NAME "Fmp4Splitter"  # Output name without extension
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    SUFFIX ".smd"  # Set the custom file extension to .smd
    PREFIX ""  # 
)
