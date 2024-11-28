# Define the plugin library
add_library(TeletextToTTML SHARED
    telx2ttml.cpp
    telx.cpp
    )

# Link dependencies if any
target_link_libraries(TeletextToTTML ${CMAKE_THREAD_LIBS_INIT})

# Include directories
target_include_directories(TeletextToTTML PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

# Set properties for the target
set_target_properties(TeletextToTTML PROPERTIES
    OUTPUT_NAME "TeletextToTTML"  # Output name without extension
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    SUFFIX ".smd"  # Set the custom file extension to .smd
    PREFIX ""  # 
)
