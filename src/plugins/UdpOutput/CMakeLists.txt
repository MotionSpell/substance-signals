# Define the plugin library (UdpOutput)
set(UdpOutput_src
    ${CMAKE_CURRENT_SOURCE_DIR}/udp_output.cpp  # Main plugin source
)

# Platform-specific socket source files
if(MSVC)
    # MSVC-specific socket file
    list(APPEND UdpOutput_src ${CMAKE_CURRENT_SOURCE_DIR}/socket_mingw.cpp)
elseif(MINGW)
    # MinGW-specific socket file
    list(APPEND UdpOutput_src ${CMAKE_CURRENT_SOURCE_DIR}/socket_mingw.cpp)
elseif(APPLE)
    # macOS/Darwin-specific socket file
    list(APPEND UdpOutput_src ${CMAKE_CURRENT_SOURCE_DIR}/socket_gnu.cpp)
elseif(UNIX)
    # Linux-specific socket file
    list(APPEND UdpOutput_src ${CMAKE_CURRENT_SOURCE_DIR}/socket_gnu.cpp)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# Add the UdpOutput plugin
add_library(UdpOutput SHARED ${UdpOutput_src})

# Link dependencies
target_link_libraries(UdpOutput
    ${CMAKE_THREAD_LIBS_INIT} 
    modules 
)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    target_link_libraries(UdpOutput ws2_32)
endif()

# Include directories
target_include_directories(UdpOutput PUBLIC
    ${SIGNALS_TOP_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}  
)

# Set properties for the target
set_target_properties(UdpOutput PROPERTIES
    OUTPUT_NAME "UdpOutput"  
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    SUFFIX ".smd"  # Set the custom file extension to .smd
    PREFIX ""  # 
)

install(TARGETS UdpOutput
	EXPORT UdpOutput
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES $<TARGET_RUNTIME_DLLS:UdpOutput> TYPE BIN)

add_custom_command(TARGET UdpOutput POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:UdpOutput> $<TARGET_RUNTIME_DLLS:UdpOutput>
  COMMAND_EXPAND_LISTS
)