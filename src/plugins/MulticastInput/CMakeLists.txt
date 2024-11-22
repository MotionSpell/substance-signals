add_library(MulticastInput STATIC
    multicast_input.cpp
    multicast_input.hpp
)

target_include_directories(MulticastInput PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Link other libraries if necessary
target_link_libraries(MulticastInput ${CMAKE_THREAD_LIBS_INIT})
