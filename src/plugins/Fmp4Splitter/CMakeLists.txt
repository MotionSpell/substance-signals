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

# Set properties for the target
set_target_properties(FMP4SPLITTER PROPERTIES
    OUTPUT_NAME "Fmp4Splitter"  # Output name without extension
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    SUFFIX ".smd"  # Set the custom file extension to .smd
    PREFIX ""  # 
)

install(TARGETS FMP4SPLITTER
	EXPORT FMP4SPLITTER
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES $<TARGET_RUNTIME_DLLS:FMP4SPLITTER> TYPE BIN)

add_custom_command(TARGET FMP4SPLITTER POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:FMP4SPLITTER> $<TARGET_RUNTIME_DLLS:FMP4SPLITTER>
  COMMAND_EXPAND_LISTS
)
