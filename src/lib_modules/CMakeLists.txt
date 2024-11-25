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
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src 
)

# Add compile flags
target_compile_options(modules PRIVATE
    -Wall -Wextra -Werror -fPIC
)

# Add linker flags
target_link_options(modules PRIVATE
    -pthread -Wl,--no-undefined
)

# Link dependencies if any
target_link_libraries(modules PRIVATE utils)

# Set output directory
set_target_properties(modules PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Installation (optional) to be tested later
install(TARGETS modules DESTINATION lib)
    