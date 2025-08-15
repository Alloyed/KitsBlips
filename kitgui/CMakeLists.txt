cmake_minimum_required(VERSION 3.22)
cmake_policy(SET CMP0048 NEW)

project(kitgui VERSION 0.0.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(../content.cmake)
include(imgui)
FetchContent_MakeAvailable(SDL3 corrade magnum magnum-plugins)

add_library(kitgui STATIC)
set_property(TARGET kitgui PROPERTY CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_enable_warnings(kitgui)

target_sources(kitgui PRIVATE
    src/context.cpp
    src/dom/domNode.cpp
    src/gfx/drawables.cpp
    src/gfx/domScene.cpp
    src/gfx/lights.cpp
    src/gfx/materials.cpp
    src/gfx/meshes.cpp
    src/imguiHelpers/knob.cpp
    src/imguiHelpers/domWrappers.cpp
    src/kitgui.cpp
)

target_include_directories(kitgui PUBLIC include)
target_include_directories(kitgui PRIVATE src)

target_link_libraries(kitgui PUBLIC
    SDL3::SDL3
    imgui
    Magnum::GL
    Magnum::Magnum
    Magnum::SceneGraph
)
target_link_libraries(kitgui PRIVATE
    imgui_opengl_sdl3
    ${MAGNUM_PLATFORMGL}
    Magnum::MeshTools
    Magnum::Shaders
    Magnum::Trade
    MagnumPlugins::StbImageImporter
    MagnumPlugins::GltfImporter
)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_sources(kitgui PRIVATE
        src/platform/platformWin32.cpp
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_sources(kitgui PRIVATE
        src/platform/platformCocoa.cpp
        src/platform/platformCocoa.m
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(X11)
    target_sources(kitgui PRIVATE
        src/platform/platformLinux.cpp
    )
    target_link_libraries(kitgui PUBLIC X11::X11 X11::Xutil)
endif()

if (PROJECT_IS_TOP_LEVEL)
    # add example exes
    set(CMAKE_CXX_STANDARD 20)
    corrade_add_resource(embeddedAssets assets/embedded.conf)
    add_executable(multiwindow examples/multiwindow.cpp ${embeddedAssets})
    target_link_libraries(multiwindow kitgui Corrade::Utility Corrade::Main)
endif()