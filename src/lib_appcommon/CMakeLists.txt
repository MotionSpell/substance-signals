set(LIB_APPCOMMON_INCS
    options.hpp
    timebomb.hpp
)
set(LIB_APPCOMMON_SRCS
    options.cpp
    safemain.cpp
    timebomb.cpp
)

add_library(appcommon STATIC ${LIB_APPCOMMON_SRCS})
add_library(signals::appcommon ALIAS appcommon)

set_target_properties(appcommon PROPERTIES
    OUTPUT_NAME signals_appcommon
    POSITION_INDEPENDENT_CODE TRUE
    )

target_include_directories(appcommon PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/signals>
    )

target_link_libraries(appcommon PUBLIC utils)

target_compile_options(appcommon PRIVATE
    -Wall -Wextra -Werror
)

# Installation (optional) to be tested later
install(TARGETS appcommon 
    EXPORT signals-appcommon
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION include/signals
)

export(
    EXPORT signals-appcommon
    NAMESPACE signals::
)

install(EXPORT signals-appcommon
    NAMESPACE signals::
    DESTINATION lib/cmake/signals
)

install(FILES ${LIB_APPCOMMON_INCS}
    DESTINATION include/signals/lib_appcommon
)
