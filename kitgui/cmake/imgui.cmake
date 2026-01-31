find_package(OpenGL REQUIRED)

add_library(imgui STATIC EXCLUDE_FROM_ALL
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
)
target_include_directories(imgui PRIVATE ${IMGUI_DIR})
target_include_directories(imgui INTERFACE ${IMGUI_DIR})

add_library(imgui_opengl STATIC EXCLUDE_FROM_ALL
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_opengl INTERFACE ${IMGUI_DIR}/backends)
target_link_libraries(imgui_opengl PUBLIC imgui OpenGL::GL)

if(KITGUI_USE_PUGL)
    #implemented in-tree
elseif(KITGUI_USE_SDL)
    FetchContent_MakeAvailable(SDL3)
    add_library(imgui_sdl3 STATIC EXCLUDE_FROM_ALL
        ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    )
    target_include_directories(imgui_sdl3 INTERFACE ${IMGUI_DIR}/backends)
    target_link_libraries(imgui_sdl3 PUBLIC imgui SDL3::SDL3)
elseif(KITGUI_USE_WIN32)
    add_library(imgui_win32 STATIC EXCLUDE_FROM_ALL
        ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    )
    target_include_directories(imgui_win32 INTERFACE ${IMGUI_DIR}/backends)
    target_link_libraries(imgui_win32 PUBLIC imgui)
elseif(KITGUI_USE_COCOA)
    add_library(imgui_cocoa STATIC EXCLUDE_FROM_ALL
        ${IMGUI_DIR}/backends/imgui_impl_osx.mm
    )
    target_include_directories(imgui_cocoa INTERFACE ${IMGUI_DIR}/backends)
    target_link_libraries(imgui_cocoa PUBLIC imgui)
endif()
