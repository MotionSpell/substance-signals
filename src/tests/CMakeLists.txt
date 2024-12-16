# List of source files for the utils library
set(LIB_UTILS_SRCS
${CMAKE_SOURCE_DIR}/src/lib_utils/clock.hpp
${CMAKE_SOURCE_DIR}/src/lib_utils/fifo.hpp
${CMAKE_SOURCE_DIR}/src/lib_utils/log.cpp
${CMAKE_SOURCE_DIR}/src/lib_utils/log.hpp
${CMAKE_SOURCE_DIR}/src/lib_utils/os.hpp
${CMAKE_SOURCE_DIR}/src/lib_utils/profiler.cpp
${CMAKE_SOURCE_DIR}/src/lib_utils/profiler.hpp
${CMAKE_SOURCE_DIR}/src/lib_utils/scheduler.cpp
${CMAKE_SOURCE_DIR}/src/lib_utils/scheduler.hpp
${CMAKE_SOURCE_DIR}/src/lib_utils/time.cpp
${CMAKE_SOURCE_DIR}/src/lib_utils/time.hpp
${CMAKE_SOURCE_DIR}/src/lib_utils/version.cpp
${CMAKE_SOURCE_DIR}/src/lib_utils/system_clock.hpp
${CMAKE_SOURCE_DIR}/src/lib_utils/sysclock.cpp
)
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    list(APPEND LIB_UTILS_SRCS ${CMAKE_SOURCE_DIR}/src/lib_utils/os_mingw.cpp)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    list(APPEND LIB_UTILS_SRCS ${CMAKE_SOURCE_DIR}/src/lib_utils/os_darwin.cpp)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    list(APPEND LIB_UTILS_SRCS ${CMAKE_SOURCE_DIR}/src/lib_utils/os_gnu.cpp)
else()
    message(ERROR "Unknown CMAKE_SYSTEM_NAME ${CMAKE_SYSTEM_NAME}")
endif()

include_directories(${BIN_DIR}
${CMAKE_SOURCE_DIR}/src 
)
list(APPEND LIB_UTILS_SRCS ${GENERATED_SIGNALS_VERSION_HEADER})

# Set the directory for the unit test source files
set(TESTSDIR ${CMAKE_CURRENT_SOURCE_DIR})
set(OUTDIR ${CMAKE_BINARY_DIR}/bin)  

file(GLOB LIB_MODULES_SRCS 
    ${CMAKE_SOURCE_DIR}/src/lib_modules/utils/*.cpp
    ${CMAKE_SOURCE_DIR}/src/lib_modules/core/*.cpp

)


file(GLOB LIB_MEDIA_SRCS 
    ${CMAKE_SOURCE_DIR}/src/lib_media/*.cpp
    ${CMAKE_SOURCE_DIR}/src/lib_media/utils/*.cpp
    ${CMAKE_SOURCE_DIR}/src/lib_media/transform/*.cpp
    ${CMAKE_SOURCE_DIR}/src/lib_media/stream/*.cpp
    ${CMAKE_SOURCE_DIR}/src/lib_media/decode/*.cpp
    ${CMAKE_SOURCE_DIR}/src/lib_media/demux/*.cpp
    ${CMAKE_SOURCE_DIR}/src/lib_media/mux/*.cpp
    ${CMAKE_SOURCE_DIR}/src/lib_media/common/*.cpp
    ${CMAKE_SOURCE_DIR}/src/lib_media/in/*.cpp
    ${CMAKE_SOURCE_DIR}/src/lib_media/encode/*.cpp
   ${CMAKE_SOURCE_DIR}/src/lib_media/out/*.cpp   
)

set(LIB_PIPELINE_SRCS
${CMAKE_SOURCE_DIR}/src/lib_pipeline/filter.cpp
${CMAKE_SOURCE_DIR}/src/lib_pipeline/filter.hpp
${CMAKE_SOURCE_DIR}/src/lib_pipeline/pipeline.cpp
${CMAKE_SOURCE_DIR}/src/lib_pipeline/pipeline.hpp
)

# Define the source files for the unit tests
set(EXE_TESTS_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/tests.cpp
    ${CMAKE_SOURCE_DIR}/src/lib_appcommon/options.cpp
    ${LIB_MEDIA_SRCS}
    ${LIB_MODULES_SRCS}
    ${LIB_PIPELINE_SRCS}
    ${LIB_UTILS_SRCS}
)

# Add any additional source files from the 'unittests' directories
file(GLOB_RECURSE UNITTEST_SOURCES "${CMAKE_SOURCE_DIR}/src/*/unittests/*.cpp")
list(APPEND EXE_TESTS_SRCS ${UNITTEST_SOURCES})

# Define the unit tests executable target
add_executable(unittests ${EXE_TESTS_SRCS})

# Link dependencies to the unittests executable 
target_link_libraries(unittests
        curl
        gpac::gpac
        libjpeg-turbo::turbojpeg
        ${FFMPEG_LIBRARIES}
        )

# Include directories for unit tests 
target_include_directories(unittests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${FFMPEG_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/lib_modules/utils 
    ${CMAKE_SOURCE_DIR}/src/lib_modules/core
    ${CMAKE_SOURCE_DIR}/src/lib_media
    ${CMAKE_SOURCE_DIR}/src/lib_media/in
    ${CMAKE_SOURCE_DIR}/src/lib_media/common
    ${CMAKE_SOURCE_DIR}/src/lib_media/out
    ${CMAKE_SOURCE_DIR}/src/lib_media/transform
    ${CMAKE_SOURCE_DIR}/src/lib_media/encode
    ${CMAKE_SOURCE_DIR}/src/lib_media/decode
    ${CMAKE_SOURCE_DIR}/src/lib_media/mux
    ${CMAKE_SOURCE_DIR}/src/lib_media/demux
    ${CMAKE_SOURCE_DIR}/src/lib_media/utils
    ${CMAKE_SOURCE_DIR}/src/lib_media/stream

)

# Set the output directory for the unittests executable inside the 'bin' folder
set_target_properties(unittests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    SUFFIX ".exe"
)