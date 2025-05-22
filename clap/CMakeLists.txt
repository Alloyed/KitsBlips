cmake_minimum_required(VERSION 3.22)
cmake_policy(SET CMP0048 NEW)

# https://github.com/libsdl-org/SDL/issues/6454
if(APPLE)
    enable_language(OBJC)
endif()


# TODO: needed for clap-wrapper, who should really be doing this for us
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Build for 10.14")

# VSTs require <= 17 for codecvt, and CLAP requires >= 17 for std::filesystem :/
set(CMAKE_CXX_STANDARD 17)

project(KitsClaps)

find_package(OpenGL REQUIRED)

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
add_library(imgui_opengl_sdl3 STATIC
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
)
target_include_directories(imgui_opengl_sdl3 INTERFACE ${IMGUI_DIR}/backends)
target_link_libraries(imgui_opengl_sdl3 PUBLIC imgui SDL3::SDL3 OpenGL::GL)

add_subdirectory(template)
