set(LIB_PIPELINE_INCS
    filter_input.hpp
    filter.hpp
    pipeline.hpp
    graph.hpp
    stats.hpp
)

set(LIB_PIPELINE_SRCS
    filter.cpp
    filter.hpp
    pipeline.cpp
    pipeline.hpp
)

add_library(pipeline STATIC ${LIB_PIPELINE_SRCS})
add_library(signals::pipeline ALIAS pipeline)

set_target_properties(pipeline PROPERTIES
    OUTPUT_NAME signals_pipeline
    POSITION_INDEPENDENT_CODE TRUE
    )

# Add include directories
target_include_directories(pipeline PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>               
    $<BUILD_INTERFACE:${SIGNALS_TOP_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include/signals>
)

## Link libraries
target_link_libraries(pipeline 
    PRIVATE 
    appcommon 
    modules
    utils
    )


# Installation (optional) to be tested later
install(TARGETS pipeline
    EXPORT signals-pipeline
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION include/signals
)

export(
    EXPORT signals-pipeline
    NAMESPACE signals::
)

install(EXPORT signals-pipeline
    NAMESPACE signals::
    DESTINATION lib/cmake/signals
)

install(FILES ${LIB_PIPELINE_INCS}
    DESTINATION include/signals/lib_pipeline
)
