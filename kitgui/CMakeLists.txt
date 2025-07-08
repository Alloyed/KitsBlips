cmake_minimum_required(VERSION 3.22)
cmake_policy(SET CMP0048 NEW)

project(kitgui VERSION 0.0.0)

set(CMAKE_CXX_STANDARD 20)

# shared libs
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(../content.cmake)
include(imgui)
FetchContent_MakeAvailable(SDL3 corrade magnum magnum-plugins)

add_library(kitgui STATIC)
set_property(TARGET kitgui PROPERTY CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

if (MSVC)
    target_compile_options(kitgui PRIVATE /W4)
else()
    target_compile_options(kitgui PRIVATE -Wall -Wextra -pedantic -Wno-unused-parameter)
endif()

target_sources(kitgui PRIVATE
    src/context.cpp
    src/gfx/drawables.cpp
    src/gfx/gltfScene.cpp
    src/gfx/lights.cpp
    src/gfx/materials.cpp
    src/gfx/meshes.cpp
    src/imgui/knob.cpp
    src/kitgui.cpp
)

target_include_directories(kitgui PUBLIC include)
target_include_directories(kitgui PUBLIC src)

corrade_add_resource(embeddedAssets assets/embedded.conf)

target_link_libraries(kitgui PUBLIC
    imgui
    imgui_opengl_sdl3
    Corrade::Main
    Magnum::GL
    ${MAGNUM_PLATFORMGL}
    Magnum::Magnum
    Magnum::MeshTools
    Magnum::SceneGraph
    Magnum::Shaders
    Magnum::Trade
    MagnumPlugins::StbImageImporter
    MagnumPlugins::GltfImporter
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
    add_executable(multiwindow examples/multiwindow.cpp
    ${embeddedAssets})
    target_link_libraries(multiwindow kitgui Corrade::Utility)
endif()