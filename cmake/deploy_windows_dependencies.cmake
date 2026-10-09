if(NOT DEFINED D1ASSIST_EXECUTABLE OR
   NOT DEFINED D1ASSIST_DEPLOY_DIRECTORY OR
   NOT DEFINED D1ASSIST_RUNTIME_DIRECTORIES)
    message(FATAL_ERROR "The Windows runtime deployment paths are required.")
endif()

if(POLICY CMP0207)
   cmake_policy(SET CMP0207 NEW)
endif()

set(scan_libraries
   "${D1ASSIST_QT_BIN_DIRECTORY}/Qt6Core.dll"
   "${D1ASSIST_QT_BIN_DIRECTORY}/Qt6Gui.dll"
   "${D1ASSIST_QT_BIN_DIRECTORY}/Qt6Widgets.dll"
   "${D1ASSIST_QT_BIN_DIRECTORY}/../share/qt6/plugins/platforms/qwindows.dll"
)

file(GET_RUNTIME_DEPENDENCIES
   LIBRARIES ${scan_libraries}
    DIRECTORIES ${D1ASSIST_RUNTIME_DIRECTORIES}
    PRE_EXCLUDE_REGEXES "api-ms-win-.*" "ext-ms-win-.*" "AzureAttest.*\\.dll"
    RESOLVED_DEPENDENCIES_VAR resolved_dependencies
    UNRESOLVED_DEPENDENCIES_VAR unresolved_dependencies
)

foreach(dependency IN LISTS unresolved_dependencies)
    message(WARNING
        "Could not resolve Windows runtime dependency; verify if required at runtime: ${dependency}"
    )
endforeach()

foreach(dependency IN LISTS resolved_dependencies)
    file(REAL_PATH "${dependency}" resolved_path)
    if(resolved_path MATCHES "^${D1ASSIST_QT_BIN_DIRECTORY}/")
        get_filename_component(filename "${resolved_path}" NAME)
        if(NOT filename MATCHES "^Qt6.*\\.dll$")
            execute_process(
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${resolved_path}"
                    "${D1ASSIST_DEPLOY_DIRECTORY}/${filename}"
                COMMAND_ERROR_IS_FATAL ANY
            )
        endif()
    endif()
endforeach()
