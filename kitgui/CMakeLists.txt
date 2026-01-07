cmake_minimum_required(VERSION 3.22)
cmake_policy(SET CMP0048 NEW)

project(kitgui VERSION 0.0.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(../content.cmake)
FetchContent_MakeAvailable(corrade magnum magnum-plugins fmt)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(KITGUI_USE_WIN32 true)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(KITGUI_USE_COCOA true)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(X11)
    FetchContent_MakeAvailable(SDL3)
    set(KITGUI_USE_SDL true)
endif()
include(imgui)

add_library(kitgui STATIC)
set_property(TARGET kitgui PROPERTY CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_enable_warnings(kitgui)

target_sources(kitgui PRIVATE
    src/context.cpp
    src/dom/domNode.cpp
    src/dom/domKnob.cpp
    src/dom/domScene.cpp
    src/fileContext.cpp
    src/gfx/drawables.cpp
    src/gfx/scene.cpp
    src/gfx/lights.cpp
    src/gfx/materials.cpp
    src/gfx/meshes.cpp
    src/controls/knob.cpp
    src/immediateMode/immediateMode.cpp
)

target_include_directories(kitgui PUBLIC include)
target_include_directories(kitgui PRIVATE src)

target_link_libraries(kitgui PUBLIC
    imgui
    Magnum::GL
    Magnum::Magnum
    Magnum::SceneGraph
    fmt::fmt

    Magnum::AnyImageImporter
    MagnumPlugins::GltfImporter
    MagnumPlugins::StbImageImporter
)
target_link_libraries(kitgui PRIVATE
    ${MAGNUM_PLATFORMGL}
    Magnum::MeshTools
    Magnum::Shaders
    Magnum::Trade
)
target_disable_warnings(Magnum::Magnum Magnum::GL Magnum::SceneGraph fmt::fmt Magnum::AnyImageImporter Magnum::GltfImporter MagnumPlugins::StdbImageImporter)

if(KITGUI_USE_SDL)
    target_compile_definitions(kitgui PUBLIC KITGUI_USE_SDL)
    target_sources(kitgui PRIVATE
        src/impl/sdl/contextImpl.cpp
    )
    target_link_libraries(kitgui PRIVATE
        SDL3::SDL3
        imgui_opengl_sdl3
    )
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(kitgui PRIVATE
            X11::X11
            X11::Xutil
        )
    endif()
elseif(KITGUI_USE_WIN32)
    target_compile_definitions(kitgui PUBLIC KITGUI_USE_WIN32)
    target_sources(kitgui PRIVATE
        src/impl/win32/contextImpl.cpp
    )
    target_link_libraries(kitgui PRIVATE
        imgui_opengl_win32
    )
elseif(KITGUI_USE_COCOA)
    target_compile_definitions(kitgui PUBLIC KITGUI_USE_COCOA)
    target_sources(kitgui PRIVATE
        src/impl/cocoa/contextImpl.cpp
    )
    target_link_libraries(kitgui PRIVATE
        imgui_opengl_cocoa
    )
endif()

if (PROJECT_IS_TOP_LEVEL)
    # add example exes
    set(CMAKE_CXX_STANDARD 20)

    add_executable(multiwindow examples/multiwindow.cpp)
    target_link_libraries(multiwindow kitgui Corrade::Utility Corrade::Main)

    add_executable(3dscene examples/3dscene.cpp)
    target_link_libraries(3dscene kitgui Corrade::Utility Corrade::Main)

    add_executable(immediatemode examples/immediatemode.cpp)
    target_link_libraries(immediatemode kitgui Corrade::Utility Corrade::Main)
endif()