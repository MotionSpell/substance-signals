# Plugin directory: SdlRender

# Define targets for the plugins
set(SDL_VIDEO_PLUGIN SDLVideo)
set(SDL_AUDIO_PLUGIN SDLAudio)

# Source files for each target
set(SDL_VIDEO_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/sdl_video.cpp
)

set(SDL_AUDIO_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/sdl_audio.cpp
)

# SDL2 package requirements
find_package(SDL2 REQUIRED)

# SDLVideo plugin target
add_library(${SDL_VIDEO_PLUGIN} SHARED ${SDL_VIDEO_SOURCES})

# Set include directories for SDLVideo
target_include_directories(${SDL_VIDEO_PLUGIN} PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link SDL2 to SDLVideo
target_link_libraries(${SDL_VIDEO_PLUGIN} PRIVATE ${SDL2_LIBRARIES})

# Set the output directory for SDLVideo
set_target_properties(${SDL_VIDEO_PLUGIN} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# SDLAudio plugin target
add_library(${SDL_AUDIO_PLUGIN} SHARED ${SDL_AUDIO_SOURCES})

# Set include directories for SDLAudio
target_include_directories(${SDL_AUDIO_PLUGIN} PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link SDL2 to SDLAudio
target_link_libraries(${SDL_AUDIO_PLUGIN} PRIVATE ${SDL2_LIBRARIES})

# Set the output directory for SDLAudio
set_target_properties(${SDL_AUDIO_PLUGIN} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Installation (optional)
install(TARGETS ${SDL_VIDEO_PLUGIN} ${SDL_AUDIO_PLUGIN} DESTINATION bin)
