# Define source files for the MPEG_DASH target
set(EXE_MPEG_DASH_SRCS
    ${CMAKE_SOURCE_DIR}/src/lib_media/common/xml.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mpeg_dash.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mpd.cpp
)
file(GLOB LIB_MODULES_SRCS 
    ${CMAKE_SOURCE_DIR}/src/lib_modules/utils/*.cpp
    ${CMAKE_SOURCE_DIR}/src/lib_modules/core/*.cpp
)


list(APPEND EXE_MPEG_DASH_SRCS
    ${LIB_MODULES_SRCS}
)

# Add the target (MPEG_DASH.smd)
add_library(MPEG_DASH SHARED ${EXE_MPEG_DASH_SRCS})

# Include directories for the module
target_include_directories(MPEG_DASH PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/
    )

# Link the necessary libraries
target_link_libraries(MPEG_DASH PRIVATE
    media modules pipeline appcommon utils
)

# Set properties for the target
set_target_properties(MPEG_DASH PROPERTIES
    OUTPUT_NAME "MPEG_DASH"  # Output name without extension
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    SUFFIX ".smd"  # Set the custom file extension to .smd
    PREFIX ""  # 
)
