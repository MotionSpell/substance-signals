cmake_minimum_required(VERSION 3.15)

project(Signals VERSION 1.0)

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

# Add all the source subdirectories
add_subdirectory(src)

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

# Add custom install targets (if needed)
#install(TARGETS dashcastx ts2ip player mcastdump mp42tsx monitor
#        RUNTIME DESTINATION bin)

set(SYSROOT_PATH "/home/sohaib/motiospell/CWI/sysroot")

include_directories(${SYSROOT_PATH}/include)

link_directories(${SYSROOT_PATH}/lib)

set(PKG_CONFIG_PATH ${SYSROOT_PATH}/lib/pkgconfig)


find_package(PkgConfig REQUIRED)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
