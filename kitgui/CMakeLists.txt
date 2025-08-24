cmake_minimum_required(VERSION 3.22)
cmake_policy(SET CMP0048 NEW)

project(kitgui VERSION 0.0.0)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(KITGUI_USE_WIN32 true)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    FetchContent_MakeAvailable(SDL3)
    set(KITGUI_USE_SDL true)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(X11)
    FetchContent_MakeAvailable(SDL3)
    set(KITGUI_USE_SDL true)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(../content.cmake)
include(imgui)
FetchContent_MakeAvailable(corrade magnum magnum-plugins)


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
)

target_include_directories(kitgui PUBLIC include)
target_include_directories(kitgui PRIVATE src)

target_link_libraries(kitgui PUBLIC
    imgui
    Magnum::GL
    Magnum::Magnum
    Magnum::SceneGraph
)
target_link_libraries(kitgui PRIVATE
    ${MAGNUM_PLATFORMGL}
    Magnum::MeshTools
    Magnum::Shaders
    Magnum::Trade
    MagnumPlugins::StbImageImporter
    MagnumPlugins::GltfImporter
)

if(KITGUI_USE_SDL)
    target_compile_definitions(kitgui PUBLIC KITGUI_USE_SDL)
    target_sources(kitgui PRIVATE
        src/impl/sdl/contextImpl.cpp
    )
    target_link_libraries(kitgui PRIVATE
        X11::X11
        X11::Xutil
        SDL3::SDL3
        imgui_opengl_sdl3
    )
elseif(KITGUI_USE_WIN32)
    target_compile_definitions(kitgui PUBLIC KITGUI_USE_WIN32)
    target_sources(kitgui PRIVATE
        src/impl/win32/contextImpl.cpp
    )
    target_link_libraries(kitgui PRIVATE
        imgui_opengl_win32
    )
endif()

if (PROJECT_IS_TOP_LEVEL)
    # add example exes
    set(CMAKE_CXX_STANDARD 20)
    corrade_add_resource(embeddedAssets assets/embedded.conf)
    add_executable(multiwindow examples/multiwindow.cpp ${embeddedAssets})
    target_link_libraries(multiwindow kitgui Corrade::Utility Corrade::Main)
endif()