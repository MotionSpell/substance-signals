set(LIB_APPCOMMON_SRCS
    options.cpp
    safemain.cpp
    timebomb.cpp
)



add_library(appcommon STATIC ${LIB_APPCOMMON_SRCS})
target_include_directories(appcommon PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    )
target_link_libraries(appcommon PRIVATE utils)

target_compile_options(appcommon PRIVATE
    -Wall -Wextra -Werror -fPIC
)