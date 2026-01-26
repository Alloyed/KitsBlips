cmake_minimum_required(VERSION 3.22)
cmake_policy(SET CMP0048 NEW)

project(kitgui VERSION 0.0.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(../content.cmake)
FetchContent_MakeAvailable(corrade magnum magnum-plugins fmt)

# Only one backend can be compiled in at a time
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    #set(KITGUI_USE_PUGL true)
    set(KITGUI_USE_WIN32 true)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    #set(KITGUI_USE_PUGL true)
    #set(KITGUI_USE_SDL true)
    set(KITGUI_USE_COCOA true)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    #set(KITGUI_USE_PUGL true)
    set(KITGUI_USE_SDL true)
endif()

include(imgui)
FetchContent_MakeAvailable(magnum-integration)

add_library(kitgui STATIC)
set_property(TARGET kitgui PROPERTY CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_enable_warnings(kitgui)

target_sources(kitgui PRIVATE
    src/context.cpp
    src/fileContext.cpp
    src/gfx/animations.cpp
    src/gfx/drawables.cpp
    src/gfx/scene.cpp
    src/gfx/lights.cpp
    src/gfx/materials.cpp
    src/gfx/meshes.cpp
    src/controls/knob.cpp
    src/theme/everforest.cpp
)

target_include_directories(kitgui PUBLIC include)
target_include_directories(kitgui PRIVATE src)

target_link_libraries(kitgui PUBLIC
    imgui
    Magnum::GL
    Magnum::Magnum
    Magnum::SceneGraph
    fmt::fmt

    MagnumIntegration::ImGui
    Magnum::AnyImageImporter
    MagnumPlugins::GltfImporter
    MagnumPlugins::StbImageImporter
)
target_link_libraries(kitgui PRIVATE
    ${MAGNUM_PLATFORMGL}
    Magnum::MeshTools
    Magnum::DebugTools
    Magnum::Shaders
    Magnum::Trade
)
target_disable_warnings(Magnum::Magnum Magnum::GL Magnum::SceneGraph fmt::fmt Magnum::AnyImageImporter Magnum::GltfImporter MagnumPlugins::StdbImageImporter)

if(KITGUI_USE_PUGL)
    include(pugl)
    target_compile_definitions(kitgui PUBLIC KITGUI_USE_PUGL)
    target_sources(kitgui PRIVATE
        src/impl/pugl/contextImpl.cpp
        src/impl/pugl/imgui_impl_pugl.cpp
    )
    target_link_libraries(kitgui PRIVATE
        pugl
        imgui_opengl
    )
elseif(KITGUI_USE_SDL)
    FetchContent_MakeAvailable(SDL3)
    target_compile_definitions(kitgui PUBLIC KITGUI_USE_SDL)
    target_sources(kitgui PRIVATE
        src/impl/sdl/contextImpl.cpp
    )
    target_link_libraries(kitgui PRIVATE
        SDL3::SDL3
        imgui_opengl
        imgui_sdl3
    )
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(X11)
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
        imgui_opengl
        imgui_win32
    )
elseif(KITGUI_USE_COCOA)
    target_compile_definitions(kitgui PUBLIC KITGUI_USE_COCOA)
    target_sources(kitgui PRIVATE
        src/impl/cocoa/contextImpl.cpp
    )
    target_link_libraries(kitgui PRIVATE
        imgui_opengl
        imgui_cocoa
    )
endif()

if (PROJECT_IS_TOP_LEVEL)
    add_subdirectory(examples)
endif()
