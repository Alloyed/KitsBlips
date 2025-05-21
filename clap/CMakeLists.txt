cmake_minimum_required(VERSION 3.22)
cmake_policy(SET CMP0048 NEW)
project(KitsClaps)

# shared libs
set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES TRUE CACHE BOOL "Download Dependencies")
include(../content.cmake)
FetchContent_MakeAvailable(clap clap-wrapper SDL3 KitDSP)

add_library(imgui STATIC
	${IMGUI_DIR}/imgui.cpp
	${IMGUI_DIR}/imgui_demo.cpp
	${IMGUI_DIR}/imgui_draw.cpp
	${IMGUI_DIR}/imgui_tables.cpp
	${IMGUI_DIR}/imgui_widgets.cpp
)
target_include_directories(imgui INTERFACE ${IMGUI_DIR})
add_library(imgui_impl STATIC
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
)
target_include_directories(imgui_impl INTERFACE ${IMGUI_DIR}/backends)
target_link_libraries(imgui_impl PUBLIC imgui)

add_subdirectory(template)
