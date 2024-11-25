# Define the plugin library
add_library(FMP4SPLITTER STATIC
    fmp4_splitter.cpp
)

# Link dependencies if any
target_link_libraries(FMP4SPLITTER ${CMAKE_THREAD_LIBS_INIT})

# Include directories
target_include_directories(FMP4SPLITTER PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)
