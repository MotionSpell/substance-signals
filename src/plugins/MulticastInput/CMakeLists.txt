# Define the plugin library (MulticastInput)
set(MulticastInput_src
    ${CMAKE_CURRENT_SOURCE_DIR}/multicast_input.cpp  # Main plugin source
)

# Platform-specific socket source files
if(MSVC)
    # MSVC-specific socket file
    list(APPEND MulticastInput_src ${CMAKE_CURRENT_SOURCE_DIR}/socket_mingw.cpp)
elseif(MINGW)
    # MinGW-specific socket file
    list(APPEND MulticastInput_src ${CMAKE_CURRENT_SOURCE_DIR}/socket_mingw.cpp)
elseif(APPLE)
    # macOS/Darwin-specific socket file
    list(APPEND MulticastInput_src ${CMAKE_CURRENT_SOURCE_DIR}/socket_gnu.cpp)
elseif(UNIX)
    # Linux-specific socket file
    list(APPEND MulticastInput_src ${CMAKE_CURRENT_SOURCE_DIR}/socket_gnu.cpp)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# Add the MulticastInput plugin
add_library(MulticastInput SHARED ${MulticastInput_src})

# Link dependencies
target_link_libraries(MulticastInput
    ${CMAKE_THREAD_LIBS_INIT}
    modules  
)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    target_link_libraries(MulticastInput ws2_32)
endif()

# Include directories
target_include_directories(MulticastInput PUBLIC
    ${SIGNALS_TOP_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}  
)

# Set properties for the target
set_target_properties(MulticastInput PROPERTIES
    OUTPUT_NAME "MulticastInput"  
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    SUFFIX ".smd"  # Set the custom file extension to .smd
    PREFIX ""  # 
)

install(TARGETS MulticastInput
	EXPORT MulticastInput
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES $<TARGET_RUNTIME_DLLS:MulticastInput> TYPE BIN)

add_custom_command(TARGET MulticastInput POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:MulticastInput> $<TARGET_RUNTIME_DLLS:MulticastInput>
  COMMAND_EXPAND_LISTS
)
