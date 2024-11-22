# Collect all test sources
file(GLOB_RECURSE UNITTEST_SOURCES
    "${CMAKE_SOURCE_DIR}/src/unittests/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests.cpp"
    "${CMAKE_SOURCE_DIR}/lib_appcommon/options.cpp"
    ${LIB_MEDIA_SRCS}
    ${LIB_MODULES_SRCS}
    ${LIB_PIPELINE_SRCS}
    ${LIB_UTILS_SRCS}
)

# Create the unit tests executable
add_executable(unittests ${UNITTEST_SOURCES})

# Include directories for the unittests
target_include_directories(unittests PRIVATE
    ${CMAKE_SOURCE_DIR}/lib_appcommon
    ${CMAKE_SOURCE_DIR}/lib_media
    ${CMAKE_SOURCE_DIR}/lib_modules
    ${CMAKE_SOURCE_DIR}/lib_pipeline
    ${CMAKE_SOURCE_DIR}/lib_utils
    ${CMAKE_SOURCE_DIR}/src/unittests
)

# Link dependencies to the unit tests
target_link_libraries(unittests PRIVATE
    appcommon
    media
    modules
    pipeline
    utils
)

# Enable testing if requested
if(BUILD_TESTING)
    enable_testing()
    add_test(NAME UnitTests COMMAND unittests)
endif()

# Set output directory for the test executable
set_target_properties(unittests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Installation (optional)
install(TARGETS unittests DESTINATION bin)
