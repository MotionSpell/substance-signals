set(LIB_APPCOMMON_SRCS
    options.cpp
    options.hpp
    safemain.cpp
    timebomb.cpp
    timebomb.hpp
)

add_library(appcommon STATIC ${LIB_APPCOMMON_SRCS})
target_include_directories(appcommon PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(appcommon PRIVATE utils)
