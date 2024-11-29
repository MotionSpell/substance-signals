# Set the directory for the unit test source files
set(TESTSDIR ${CMAKE_CURRENT_SOURCE_DIR})
set(OUTDIR ${CMAKE_BINARY_DIR}/bin)  

file(GLOB LIB_MODULES_SRCS 
    ${CMAKE_SOURCE_DIR}/src/lib_modules/utils/*.cpp
    ${CMAKE_SOURCE_DIR}/src/lib_modules/core/*.cpp
)


file(GLOB LIB_MEDIA_SRCS 
    ${CMAKE_SOURCE_DIR}/src/lib_media/*.cpp
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
    appcommon  
    media
    modules
    pipeline
    utils
    curl
    FFMPEG::AVCODEC
)

# Include directories for unit tests 
target_include_directories(unittests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# Set the output directory for the unittests executable inside the 'bin' folder
set_target_properties(unittests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    SUFFIX ".exe"
)