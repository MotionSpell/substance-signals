set(LIB_APPCOMMON_SRCS
    options.cpp
    safemain.cpp
    timebomb.cpp
)



add_library(appcommon STATIC ${LIB_APPCOMMON_SRCS})
target_include_directories(appcommon PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(appcommon PRIVATE utils)

target_compile_options(appcommon PRIVATE
    -Wall -Wextra -Werror -fPIC
)

#******* experiment
# Add alias for installation
add_library(Signals::appcommon ALIAS appcommon)

# Install the library
install(TARGETS appcommon EXPORT SignalsTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/Signals/media
    FILES_MATCHING PATTERN "*.h"
)
#********* end of experiment. 
