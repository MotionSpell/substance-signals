set(LIB_PIPELINE_SRCS
    filter.cpp
    filter.hpp
    pipeline.cpp
    pipeline.hpp
)

add_library(pipeline STATIC ${LIB_PIPELINE_SRCS})

# Add include directories
target_include_directories(pipeline PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}                
    ${CMAKE_SOURCE_DIR}/src
)

# Link libraries
target_link_libraries(pipeline PRIVATE appcommon utils)


#******* experiment
# Add alias for installation
add_library(Signals::pipeline ALIAS pipeline)

# Install the library
install(TARGETS pipeline EXPORT SignalsTargets
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
