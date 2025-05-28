# List of source files for the utils library
set(LIB_UTILS_INCS
    clock.hpp
    fifo.hpp
    fraction.hpp
    log.hpp
    os.hpp
    profiler.hpp
    scheduler.hpp
    time.hpp
    tools.hpp
    small_map.hpp
    system_clock.hpp
)
set(LIB_UTILS_SRCS
    log.cpp
    profiler.cpp
    scheduler.cpp
    time.cpp
    version.cpp
    sysclock.cpp
    json.cpp
    syslog.cpp
    timer.cpp
)
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    list(APPEND LIB_UTILS_SRCS os_mingw.cpp)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    list(APPEND LIB_UTILS_SRCS os_darwin.cpp)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    list(APPEND LIB_UTILS_SRCS os_gnu.cpp)
else()
    message(ERROR "Unknown CMAKE_SYSTEM_NAME ${CMAKE_SYSTEM_NAME}")
endif()

# Print out the current source dir and parent dir for debugging
message(STATUS "lib_utils CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "lib_utils parent dir: ${CMAKE_CURRENT_SOURCE_DIR}/..")
message(STATUS "lib_utils include headers: ${LIB_UTILS_INCS}")

# Print out the resolved absolute paths for headers
foreach(header ${LIB_UTILS_INCS})
    get_filename_component(HEADER_ABS "${CMAKE_CURRENT_SOURCE_DIR}/${header}" ABSOLUTE)
    message(STATUS "lib_utils header: ${HEADER_ABS}")
endforeach()

add_library(utils SHARED)
add_library(signals::utils ALIAS utils)

signals_install_library(utils SHARED)

target_sources(utils PRIVATE 
    ${LIB_UTILS_SRCS}
    ${LIB_UTILS_INCS}
    )

target_sources(utils INTERFACE 
    $<BUILD_INTERFACE:${LIB_UTIL_INCS}>
    $<INSTALL_INTERFACE:$<LIST:TRANSFORM,${LIB_UTIL_INCS},PREPEND,include/signals/lib_utils/>>
    )


# Platform-specific adjustments
if(APPLE)
    # On macOS, we need to include the os_darwin.cpp file
    list(APPEND LIB_UTILS_SRCS os_darwin.cpp)
    target_link_libraries(utils PRIVATE "-ldl" rapidjson)
elseif(UNIX AND NOT APPLE)
    # For Linux/Unix, include os_gnu.cpp and link against necessary libraries
    list(APPEND LIB_UTILS_SRCS os_gnu.cpp)
    target_link_libraries(utils PRIVATE "-ldl" "-lrt" rapidjson)
elseif(WIN32)
    # For Windows, include os_mingw.cpp (if needed for MinGW)
    list(APPEND LIB_UTILS_SRCS os_mingw.cpp)
endif()

# Specify include directories for the utils target
target_include_directories(utils PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${SIGNALS_VERSION_HEADER_DIR}>
    $<INSTALL_INTERFACE:include/signals>
)

# Print out the include directories for the utils target
get_target_property(utils_includes utils INCLUDE_DIRECTORIES)
message(STATUS "utils target include directories: ${utils_includes}")

install(FILES ${LIB_UTILS_INCS}
    DESTINATION include/signals/lib_utils
)
