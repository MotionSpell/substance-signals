# Define the SDLVideo plugin
set(SDLVideo_src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdl_video.cpp  # Main source for SDLVideo
)

# Link SDL2 package for SDLVideo
find_package(SDL2 REQUIRED)

# Define the SDLVideo target
add_library(SDLVideo SHARED ${SDLVideo_src})

# Link dependencies
target_link_libraries(SDLVideo
    ${CMAKE_THREAD_LIBS_INIT}  
    modules
    SDL2::SDL2  
)

# Include directories
target_include_directories(SDLVideo PUBLIC
    ${SIGNALS_TOP_SOURCE_DIR}/src
    ${SDL2_INCLUDE_DIRS}
)

# Set properties for the target
set_target_properties(SDLVideo PROPERTIES
    OUTPUT_NAME "SDLVideo"  # Output name without extension
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    SUFFIX ".smd"  # Set the custom file extension to .smd
    PREFIX ""  # 
)

install(TARGETS SDLVideo
	EXPORT SDLVideo
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_custom_command(TARGET SDLVideo POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:SDLVideo> $<TARGET_RUNTIME_DLLS:SDLVideo>
  COMMAND_EXPAND_LISTS
)

# Define the SDLAudio plugin
set(SDLAudio_src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdl_audio.cpp  
)

# Define the SDLAudio target
add_library(SDLAudio SHARED ${SDLAudio_src})

# Link dependencies
target_link_libraries(SDLAudio
    ${CMAKE_THREAD_LIBS_INIT} 
    modules 
    SDL2::SDL2  # Link SDL2
)

# Include directories for SDLAudio
target_include_directories(SDLAudio PUBLIC
    ${SIGNALS_TOP_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/SdlRender
    ${SDL2_INCLUDE_DIRS}
)

# Set properties for SDLAudio target
set_target_properties(SDLAudio PROPERTIES
    OUTPUT_NAME "SDLAudio"  # Output name without extension
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    SUFFIX ".smd"  # Set the custom file extension to .smd
    PREFIX ""  # 
)

install(TARGETS SDLAudio
	EXPORT SDLAudio
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_custom_command(TARGET SDLAudio POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:SDLAudio> $<TARGET_RUNTIME_DLLS:SDLAudio>
  COMMAND_EXPAND_LISTS
)
