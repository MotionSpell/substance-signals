# CMakeLists for lib_signals

# Add the library target
add_library(lib_signals INTERFACE)

# Specify include directories for the library
target_include_directories(lib_signals
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/lib_appcommon
)

# Link dependencies for lib_signals
target_link_libraries(lib_signals
    INTERFACE
        lib_appcommon
        lib_media
        lib_modules
        lib_pipeline
        lib_utils
)

# Add unit tests
if(BUILD_TESTING)
    file(GLOB UNIT_TEST_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/unittests/*.cpp
    )

    add_executable(lib_signals_unittests
        ${UNIT_TEST_SOURCES}
        ${CMAKE_CURRENT_SOURCE_DIR}/../lib_appcommon/options.cpp
    )

    target_link_libraries(lib_signals_unittests
        PRIVATE
            lib_signals
            lib_appcommon
            lib_media
            lib_modules
            lib_pipeline
            lib_utils
    )

    # Add test to CTest
    add_test(
        NAME lib_signals_unittests
        COMMAND lib_signals_unittests
    )
endif()
