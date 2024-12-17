set(LIB_APPCOMMON_SRCS
    options.cpp
    safemain.cpp
    timebomb.cpp
)



add_library(appcommon STATIC ${LIB_APPCOMMON_SRCS})

set_target_properties(appcommon PROPERTIES
    OUTPUT_NAME signals_appcommon
    POSITION_INDEPENDENT_CODE TRUE
    )

target_include_directories(appcommon PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    )
target_link_libraries(appcommon PUBLIC utils)

target_compile_options(appcommon PRIVATE
    -Wall -Wextra -Werror
)


# Installation (optional) to be tested later
install(TARGETS appcommon 
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION include/signals
)