# Define the plugin target
set(PLUGIN_NAME MPEG_DASH)

# Add source files
set(SOURCES
    ${CMAKE_SOURCE_DIR}/src/lib_media/common/xml.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mpeg_dash.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mpd.cpp
)

# Add the plugin as a library or executable as per requirements
# Here we assume it's a shared library based on `.smd` extension
add_library(${PLUGIN_NAME} SHARED ${SOURCES})

# Include directories
target_include_directories(${PLUGIN_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link dependencies (if any)
# Replace `your_dependency` with actual targets if needed
# target_link_libraries(${PLUGIN_NAME} PRIVATE your_dependency)

# Set the output directory for the plugin
set_target_properties(${PLUGIN_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Installation 
install(TARGETS ${PLUGIN_NAME} DESTINATION bin)
