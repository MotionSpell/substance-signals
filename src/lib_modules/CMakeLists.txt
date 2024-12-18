# Define the library target
add_library(modules SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/core/allocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/connection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/data.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/factory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/loader.cpp
)

# Add include directories
target_include_directories(modules PRIVATE
    ${CMAKE_SOURCE_DIR}/src/ 
    ${CMAKE_SOURCE_DIR}/src/lib_modules/utils 
    ${CMAKE_SOURCE_DIR}/src/lib_modules/core

)

# Add compile flags
target_compile_options(modules PUBLIC
    -Wall -Wextra -Werror -fPIC
)

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    target_link_options(modules PUBLIC
    -pthread -Wl,--no-undefined
    )
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    # xxxjack -undefined error is deprecated
    #target_link_options(modules PUBLIC
    #-undefined error
    #)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    target_link_options(modules PUBLIC
    -pthread -Wl,--no-undefined
    )
else()
    message(ERROR "Unknown CMAKE_SYSTEM_NAME ${CMAKE_SYSTEM_NAME}")
endif()
# Add linker flags

# Link dependencies if any
target_link_libraries(modules PUBLIC utils)

# Set output directory
set_target_properties(modules PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

#******* experiment
# Add alias for installation
add_library(Signals::modules ALIAS modules)

# Install the library
install(TARGETS modules EXPORT SignalsTargets
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

    