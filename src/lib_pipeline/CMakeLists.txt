set(LIB_PIPELINE_SRCS
    filter.cpp
    filter.hpp
    pipeline.cpp
    pipeline.hpp
)

add_library(pipeline STATIC ${LIB_PIPELINE_SRCS})
target_include_directories(pipeline PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pipeline PRIVATE appcommon utils)
