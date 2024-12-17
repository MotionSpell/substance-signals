# List of source files for the utils library
set(LIB_UTILS_SRCS
    clock.hpp
    fifo.hpp
    log.cpp
    log.hpp
    os.hpp
    profiler.cpp
    profiler.hpp
    scheduler.cpp
    scheduler.hpp
    time.cpp
    time.hpp
    version.cpp
    system_clock.hpp
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

include_directories(
    ${BIN_DIR}
    ${CMAKE_SOURCE_DIR}/src 
)
list(APPEND LIB_UTILS_SRCS ${GENERATED_SIGNALS_VERSION_HEADER})

add_library(utils STATIC ${LIB_UTILS_SRCS})

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
    ${CMAKE_CURRENT_SOURCE_DIR}  
    ${CMAKE_CURRENT_SOURCE_DIR}/lib_utils  
    ${BIN_DIR}  
)

# Installation (optional) to be tested later
install(TARGETS utils 
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION include/signals
)
