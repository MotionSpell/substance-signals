# Define the plugin library
add_library(TeletextToTTML SHARED
    telx2ttml.cpp
    telx.cpp
    )

# Link dependencies if any
target_link_libraries(TeletextToTTML 
    ${CMAKE_THREAD_LIBS_INIT} 
    modules
    ${FFMPEG_LIBRARIES}
    )

# Include directories
target_include_directories(TeletextToTTML 
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${FFMPEG_INCLUDE_DIRS}
)

# Set properties for the target
set_target_properties(TeletextToTTML PROPERTIES
    OUTPUT_NAME "TeletextToTTML"  # Output name without extension
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    SUFFIX ".smd"  # Set the custom file extension to .smd
    PREFIX ""  # 
)

install(TARGETS TeletextToTTML
	EXPORT TeletextToTTML
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_custom_command(TARGET TeletextToTTML POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:TeletextToTTML> $<TARGET_RUNTIME_DLLS:TeletextToTTML>
  COMMAND_EXPAND_LISTS
)
