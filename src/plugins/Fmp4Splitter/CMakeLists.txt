# Define the plugin library
add_library(FMP4SPLITTER SHARED
    fmp4_splitter.cpp
)

# Link dependencies if any
target_link_libraries(FMP4SPLITTER 
    ${CMAKE_THREAD_LIBS_INIT}
    modules
    )

# Include directories
target_include_directories(FMP4SPLITTER PUBLIC
    ${SIGNALS_TOP_SOURCE_DIR}/src
)

signals_install_plugin(FMP4SPLITTER)
