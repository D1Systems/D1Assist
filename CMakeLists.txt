cmake_minimum_required(VERSION 3.20)

project(D1Assist VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)

if(WIN32)
    enable_language(RC)
endif()

if(MINGW)
    add_link_options(-static -static-libgcc -static-libstdc++)
endif()

if(WIN32)
    set(D1ASSIST_TRANSPORT_SOURCE src/platform/windows/tcp_connection_windows.cpp)
else()
    set(D1ASSIST_TRANSPORT_SOURCE src/platform/linux/tcp_connection_posix.cpp)
endif()

add_library(d1assist_core
    src/core/device_identity.cpp
    src/core/password_manager.cpp
    src/core/packet.cpp
    ${D1ASSIST_TRANSPORT_SOURCE}
)

target_include_directories(d1assist_core PUBLIC include)

if(WIN32)
    target_link_libraries(d1assist_core PUBLIC ws2_32)
endif()

if(MSVC)
    target_compile_options(d1assist_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(d1assist_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

qt_add_executable(D1Assist WIN32
    src/app/main.cpp
    src/app/main_window.cpp
    src/app/main_window.h
)
if(WIN32)
    target_sources(D1Assist PRIVATE resources/D1Assist.rc)
endif()
target_link_libraries(D1Assist PRIVATE d1assist_core Qt6::Widgets)
add_custom_command(
    TARGET D1Assist
    POST_BUILD
    COMMAND
        "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/D1AssistLogo.png"
        "$<TARGET_FILE_DIR:D1Assist>/D1AssistLogo.png"
    VERBATIM
)

if(WIN32)
    get_filename_component(D1ASSIST_QT_PREFIX "${Qt6_DIR}/../../.." ABSOLUTE)
    find_program(
        D1ASSIST_WINDEPLOYQT
        NAMES windeployqt
        HINTS "${D1ASSIST_QT_PREFIX}/bin"
    )
    if(NOT D1ASSIST_WINDEPLOYQT)
        message(FATAL_ERROR "windeployqt is required to deploy the Qt runtime for D1Assist.")
    endif()

    add_custom_command(
        TARGET D1Assist
        POST_BUILD
        COMMAND
            "${D1ASSIST_WINDEPLOYQT}"
            --no-translations
            --compiler-runtime
            --dir "$<TARGET_FILE_DIR:D1Assist>"
            "$<TARGET_FILE:D1Assist>"
        VERBATIM
    )

    get_filename_component(D1ASSIST_QT_BIN_DIRECTORY "${D1ASSIST_QT_PREFIX}/bin" ABSOLUTE)
    set(D1ASSIST_RUNTIME_DIRECTORIES "${D1ASSIST_QT_BIN_DIRECTORY}")
    if(MINGW)
        get_filename_component(D1ASSIST_MINGW_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
        list(APPEND D1ASSIST_RUNTIME_DIRECTORIES "${D1ASSIST_MINGW_BIN}")
        foreach(D1ASSIST_RUNTIME_DLL IN ITEMS
                libgcc_s_seh-1.dll
                libstdc++-6.dll
                libwinpthread-1.dll)
            add_custom_command(
                TARGET D1Assist
                POST_BUILD
                COMMAND
                    "${CMAKE_COMMAND}" -E copy_if_different
                    "${D1ASSIST_MINGW_BIN}/${D1ASSIST_RUNTIME_DLL}"
                    "$<TARGET_FILE_DIR:D1Assist>/${D1ASSIST_RUNTIME_DLL}"
                VERBATIM
            )
        endforeach()
    endif()

    add_custom_command(
        TARGET D1Assist
        POST_BUILD
        COMMAND
            "${CMAKE_COMMAND}"
            "-DD1ASSIST_EXECUTABLE=$<TARGET_FILE:D1Assist>"
            "-DD1ASSIST_DEPLOY_DIRECTORY=$<TARGET_FILE_DIR:D1Assist>"
            "-DD1ASSIST_RUNTIME_DIRECTORIES=${D1ASSIST_RUNTIME_DIRECTORIES}"
            "-DD1ASSIST_QT_BIN_DIRECTORY=${D1ASSIST_QT_BIN_DIRECTORY}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/deploy_windows_dependencies.cmake"
        VERBATIM
    )
endif()

add_subdirectory(server)
