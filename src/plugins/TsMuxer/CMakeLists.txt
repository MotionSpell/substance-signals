# Define the plugin library
add_library(TsMuxer SHARED
  mpegts_muxer.cpp
  pes.cpp
  crc.cpp
    )

# Link dependencies if any
target_link_libraries(TsMuxer 
    ${CMAKE_THREAD_LIBS_INIT}
    modules
    )

# Include directories
target_include_directories(TsMuxer PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

# Set properties for the target
set_target_properties(TsMuxer PROPERTIES
    OUTPUT_NAME "TsMuxer"  # Output name without extension
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    SUFFIX ".smd"  # Set the custom file extension to .smd
    PREFIX ""  # 
)

install(TARGETS TsMuxer
	EXPORT TsMuxer
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_custom_command(TARGET TsMuxer POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:TsMuxer> $<TARGET_RUNTIME_DLLS:TsMuxer>
  COMMAND_EXPAND_LISTS
)
