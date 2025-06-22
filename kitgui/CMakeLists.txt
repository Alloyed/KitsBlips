cmake_minimum_required(VERSION 3.22)
cmake_policy(SET CMP0048 NEW)

project(kitgui VERSION 0.0.0)

set(CMAKE_CXX_STANDARD 20)

# shared libs
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(../content.cmake)
include(imgui)
include(glad)
FetchContent_MakeAvailable(battery-embed)
FetchContent_MakeAvailable(SDL3 glm tinygltf)

add_library(kitgui STATIC)

target_compile_definitions(kitgui PUBLIC KITSBLIPS_ENABLE_GUI=1)
if (MSVC)
    target_compile_options(kitgui PRIVATE /W4)
else()
    target_compile_options(kitgui PRIVATE -Wall -Wextra -pedantic -Wno-unused-parameter)
endif()

target_sources(kitgui PRIVATE
    src/context.cpp
    src/kitgui.cpp
    #src/scene3d.cpp
    #src/shaders.cpp
    #src/imgui/knob.cpp
)

target_include_directories(kitgui PUBLIC include)
target_include_directories(kitgui PUBLIC src)

b_embed(kitgui assets/base.vert)
b_embed(kitgui assets/base.frag)
b_embed(kitgui assets/duck.glb)
target_link_libraries(kitgui PUBLIC
    imgui
    imgui_opengl_sdl3
    glm
    tinygltf
    glad
)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_sources(kitgui PUBLIC
        src/platform/platformWin32.cpp
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_sources(kitgui PUBLIC
        src/platform/platformCocoa.cpp
        src/platform/platformCocoa.m
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(X11)
    target_sources(kitgui PUBLIC
        src/platform/platformLinux.cpp
    )
    target_link_libraries(kitgui PUBLIC X11::X11 X11::Xutil)
endif()

if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    # add example exes
    add_executable(multiwindow examples/multiwindow.cpp)
    target_link_libraries(multiwindow kitgui)
endif()