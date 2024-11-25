# Add the library target
add_library(lib_signals INTERFACE)

# Specify include directories for the library
target_include_directories(lib_signals
    INTERFACE
        ${CMAKE_SOURCE_DIR}/src/tests
        ${CMAKE_SOURCE_DIR}/src

)

# Link dependencies for lib_signals
target_link_libraries(lib_signals
    INTERFACE
        appcommon
        media
        modules
        pipeline
        utils
)



# Add unit tests
if(BUILD_TESTING)
    file(GLOB UNIT_TEST_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/unittests/*.cpp
        ${CMAKE_SOURCE_DIR}/src/tests/tests.cpp
    )

    add_executable(lib_signals_unittests
        ${UNIT_TEST_SOURCES}
        ${CMAKE_CURRENT_SOURCE_DIR}/../lib_appcommon/options.cpp
    )
    target_include_directories(lib_signals_unittests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/lib_appcommon
    ${CMAKE_SOURCE_DIR}/src/tests

)


    target_link_libraries(lib_signals_unittests
        PRIVATE
            lib_signals
            appcommon
            media
            modules
            pipeline
            utils
    )

    # Add test to CTest
    add_test(
        NAME lib_signals_unittests
        COMMAND lib_signals_unittests
    )
endif()
