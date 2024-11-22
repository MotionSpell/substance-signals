file(GLOB_RECURSE LIB_MEDIA_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/**/*.cpp")

add_library(media STATIC ${LIB_MEDIA_SRCS})
target_include_directories(media PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(media PRIVATE modules pipeline appcommon utils)
