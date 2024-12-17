set(LIB_PIPELINE_SRCS
    filter.cpp
    filter.hpp
    pipeline.cpp
    pipeline.hpp
)

add_library(pipeline STATIC ${LIB_PIPELINE_SRCS})

set_target_properties(pipeline PROPERTIES
    OUTPUT_NAME signals_pipeline
    POSITION_INDEPENDENT_CODE TRUE
    )

# Add include directories
target_include_directories(pipeline PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}                
    ${CMAKE_SOURCE_DIR}/src
)

# Link libraries
target_link_libraries(pipeline 
    PUBLIC 
    appcommon 
    utils
    )


# Installation (optional) to be tested later
install(TARGETS pipeline 
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION include/signals
)