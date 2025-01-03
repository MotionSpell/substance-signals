if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    add_compile_options(-Wall -Wextra -Werror -fvisibility=default -fvisibility-inlines-hidden -Wno-deprecated-declarations)
    # xxxjack this is a hack. We need to find a better way to
    # find the C++ and other system libraries.
    set(CMAKE_MINGW_SYSTEM_LIBRARY_PATH "C:/msys64/ucrt64/bin")
endif()

macro(signals_install_plugin _component)

set_target_properties(${_component} PROPERTIES 
    OUTPUT_NAME "${_component}" 
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin 
    SUFFIX ".smd" 
    PREFIX ""
    )

install(TARGETS ${_component}
    RUNTIME_DEPENDENCY_SET ${_component}_Deps
	EXPORT ${_component}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    FRAMEWORK DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(RUNTIME_DEPENDENCY_SET ${_component}_Deps
    PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-"
    POST_EXCLUDE_REGEXES ".*system32/.*\\.dll" "Frameworks"
    DIRECTORIES ${CMAKE_SYSTEM_LIBRARY_PATH} ${CMAKE_MINGW_SYSTEM_LIBRARY_PATH}
)
#install(FILES $<TARGET_RUNTIME_DLLS:${_component}> TYPE BIN)
#
#add_custom_command(TARGET ${_component} POST_BUILD
#  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:${_component}> $<TARGET_RUNTIME_DLLS:${_component}>
#  COMMAND_EXPAND_LISTS
#)

endmacro()

macro(signals_install_app _component)

set_target_properties(${_component} PROPERTIES 
    OUTPUT_NAME "${_component}" 
    SUFFIX ".exe" 
    )

install(TARGETS ${_component}
    RUNTIME_DEPENDENCY_SET ${_component}_Deps
	EXPORT ${_component}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(RUNTIME_DEPENDENCY_SET ${_component}_Deps
    PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-"
    POST_EXCLUDE_REGEXES ".*system32/.*\\.dll" "Frameworks"
    DIRECTORIES ${CMAKE_SYSTEM_LIBRARY_PATH} ${CMAKE_MINGW_SYSTEM_LIBRARY_PATH}
)
#install(FILES $<TARGET_RUNTIME_DLLS:${_component}> TYPE BIN)
#
#add_custom_command(TARGET ${_component} POST_BUILD
#  COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:${_component}> $<TARGET_RUNTIME_DLLS:${_component}>
#  COMMAND_EXPAND_LISTS
#)

endmacro()