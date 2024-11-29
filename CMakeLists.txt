cmake_minimum_required(VERSION 3.15)

project(Signals VERSION 1.0)
# Add extension directories (for things like Find<package>)
set(CMAKE_MODULE_PATH
	${CMAKE_CURRENT_LIST_DIR}/CMakeFiles
	${CMAKE_MODULE_PATH}
	)

    # Set the C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Global compiler options
add_compile_options(-Wall -Wextra -Werror -fvisibility=hidden -fvisibility-inlines-hidden -Wno-deprecated-declarations)

# Debug-specific options
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g3)
endif()

# Release-specific options
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-w -DNDEBUG)
    add_link_options(-s)
endif()

# Define paths for source and scripts
set(SCRIPTS_DIR "${CMAKE_SOURCE_DIR}/scripts")
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(BIN_DIR "${CMAKE_BINARY_DIR}/bin")

file(MAKE_DIRECTORY ${BIN_DIR})

# Generate signals_version.h using version.sh
add_custom_command(
    OUTPUT ${BIN_DIR}/signals_version.h
    COMMAND ${CMAKE_COMMAND} -E echo "Working directory: ${CMAKE_BINARY_DIR}"
    COMMAND ${SCRIPTS_DIR}/version.sh
    COMMAND ${CMAKE_COMMAND} -E echo "Version script executed"
    COMMAND ${SCRIPTS_DIR}/version.sh > ${BIN_DIR}/signals_version.h
    DEPENDS ${SCRIPTS_DIR}/version.sh
    COMMENT "Generating signals_version.h"
)
# Add a custom target to trigger the custom command
add_custom_target(generate_signals_version_header ALL DEPENDS ${BIN_DIR}/signals_version.h)


# Ensure the generated signals_version.h is available globally
set(SIGNALS_VERSION_HEADER "${BIN_DIR}/signals_version.h")

# Include directories for global availability
include_directories(${BIN_DIR})

# make the version header available as part of a global variable
# to be  accessible in other sub-projects or libraries
set(GENERATED_SIGNALS_VERSION_HEADER ${SIGNALS_VERSION_HEADER})

message(STATUS "Generated signals_version.h available at ${SIGNALS_VERSION_HEADER}")

# Check if SYSROOT_PATH is set via an environment variable
if(DEFINED ENV{SYSROOT_PATH})
    set(SYSROOT_PATH $ENV{SYSROOT_PATH})
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/sysroot")
    # If not set, use a default relative path or fail gracefully
    set(SYSROOT_PATH "${CMAKE_SOURCE_DIR}/sysroot")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/../sysroot")
    # If not set, use a default relative path or fail gracefully
    set(SYSROOT_PATH "${CMAKE_SOURCE_DIR}/../sysroot")
endif()

# Ensure SYSROOT_PATH is valid
if(NOT EXISTS ${SYSROOT_PATH})
    message(FATAL_ERROR "SYSROOT_PATH does not exist or is invalid: ${SYSROOT_PATH}")
endif()

message(STATUS "Using SYSROOT_PATH: ${SYSROOT_PATH}")

# Add the include and lib directories from SYSROOT_PATH
#include_directories(${SYSROOT_PATH}/include)
# xxxjack temporarily re-enabling this one:
link_directories(${SYSROOT_PATH}/lib)

# Set pkg-config path
set(ENV{PKG_CONFIG_PATH} "${SYSROOT_PATH}/lib/pkgconfig")



find_package(PkgConfig REQUIRED)
find_package(SDL2 REQUIRED)
find_package(libjpeg-turbo REQUIRED)
find_package(FFmpeg REQUIRED COMPONENTS AVUTIL AVFILTER AVCODEC AVFORMAT AVDEVICE SWSCALE SWRESAMPLE)
find_package(GPAC)
if (NOT GPAC_FOUND)
    # Find it using pkgconfig, for now
    pkg_check_modules(GPAC REQUIRED gpac)
endif()


set(CMAKE_POSITION_INDEPENDENT_CODE ON)


# Add all the source subdirectories
add_subdirectory(src)

# Add custom targets for easy builds
add_custom_target(build_all
    DEPENDS
        utils
        appcommon
        pipeline
        modules
        media
        plugins
        dashcastx
        #ts2ip
        player
        mcastdump
        mp42tsx
        monitor
        unittests
    COMMENT "Building all targets."
)
