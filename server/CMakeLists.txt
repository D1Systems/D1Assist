add_executable(D1AssistServer
    src/main.cpp
    src/device_registry.cpp
)

target_include_directories(D1AssistServer PRIVATE src)
target_link_libraries(D1AssistServer PRIVATE d1assist_core)

if(MSVC)
    target_compile_options(D1AssistServer PRIVATE /W4 /permissive-)
else()
    target_compile_options(D1AssistServer PRIVATE -Wall -Wextra -Wpedantic)
endif()
