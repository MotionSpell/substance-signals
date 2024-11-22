file(GLOB_RECURSE APPS_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/**/*.cpp")

add_executable(dashcastx dashcastx/main.cpp)
target_link_libraries(dashcastx PRIVATE plugins media modules pipeline appcommon utils)

add_executable(ts2ip ts2ip/main.cpp)
target_link_libraries(ts2ip PRIVATE plugins media modules pipeline appcommon utils)

add_executable(player player/player.cpp player/pipeliner_player.cpp)
target_link_libraries(player PRIVATE plugins media modules pipeline appcommon utils)

add_executable(mcastdump mcastdump/main.cpp)
target_link_libraries(mcastdump PRIVATE plugins media modules pipeline appcommon utils)

add_executable(mp42tsx mp42tsx/mp42tsx.cpp mp42tsx/options.cpp mp42tsx/pipeliner_mp42ts.cpp)
target_link_libraries(mp42tsx PRIVATE plugins media modules pipeline appcommon utils)

add_executable(monitor monitor/main.cpp)
target_link_libraries(monitor PRIVATE plugins media modules pipeline appcommon utils)
