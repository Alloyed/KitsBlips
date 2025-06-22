find_package(OpenGL REQUIRED)
FetchContent_MakeAvailable(SDL3)

add_library(imgui STATIC EXCLUDE_FROM_ALL
	${IMGUI_DIR}/imgui.cpp
	${IMGUI_DIR}/imgui_demo.cpp
	${IMGUI_DIR}/imgui_draw.cpp
	${IMGUI_DIR}/imgui_tables.cpp
	${IMGUI_DIR}/imgui_widgets.cpp
)
target_include_directories(imgui INTERFACE ${IMGUI_DIR})
add_library(imgui_opengl_sdl3 STATIC EXCLUDE_FROM_ALL
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
)

target_include_directories(imgui_opengl_sdl3 INTERFACE ${IMGUI_DIR}/backends)
target_link_libraries(imgui_opengl_sdl3 PUBLIC imgui SDL3::SDL3 OpenGL::GL)
