# List of source files for the utils library
set(LIB_UTILS_INCS
    clock.hpp
    fifo.hpp
    log.hpp
    os.hpp
    profiler.hpp
    scheduler.hpp
    time.hpp
    system_clock.hpp
)
set(LIB_UTILS_SRCS
    log.cpp
    profiler.cpp
    scheduler.cpp
    time.cpp
    version.cpp
    sysclock.cpp
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

add_library(utils STATIC ${LIB_UTILS_SRCS})
target_sources(utils PUBLIC ${LIB_UTILS_INCS})

set_target_properties(utils PROPERTIES
    OUTPUT_NAME signals_utils
    POSITION_INDEPENDENT_CODE TRUE
    )

# Platform-specific adjustments
if(APPLE)
    # On macOS, we need to include the os_darwin.cpp file
    list(APPEND LIB_UTILS_SRCS os_darwin.cpp)
    target_link_libraries(utils PRIVATE "-ldl")
elseif(UNIX AND NOT APPLE)
    # For Linux/Unix, include os_gnu.cpp and link against necessary libraries
    list(APPEND LIB_UTILS_SRCS os_gnu.cpp)
    target_link_libraries(utils PRIVATE "-ldl" "-lrt")
elseif(WIN32)
    # For Windows, include os_mingw.cpp (if needed for MinGW)
    list(APPEND LIB_UTILS_SRCS os_mingw.cpp)
endif()

set(BIN_DIR ${CMAKE_BINARY_DIR}/bin)

# Specify include directories for the utils target
target_include_directories(utils PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib_utils>
    $<BUILD_INTERFACE:${SIGNALS_VERSION_HEADER_DIR}>
    $<INSTALL_INTERFACE:include/signals>
)

# Installation (optional) to be tested later
install(TARGETS utils 
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION include/signals
)

install(FILES ${LIB_UTILS_INCS}
    DESTINATION include/signals/lib_utils
)
