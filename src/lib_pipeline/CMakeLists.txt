set(LIB_PIPELINE_SRCS
    filter.cpp
    filter.hpp
    pipeline.cpp
    pipeline.hpp
)

add_library(pipeline STATIC ${LIB_PIPELINE_SRCS})

# Add include directories
target_include_directories(pipeline PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}                
    ${CMAKE_SOURCE_DIR}/src/lib_modules 
    ${CMAKE_SOURCE_DIR}/src/lib_signals       
    ${CMAKE_SOURCE_DIR}/src/lib_utils
)

# Link libraries
target_link_libraries(pipeline PRIVATE appcommon utils)
