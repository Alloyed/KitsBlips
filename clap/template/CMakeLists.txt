find_package(OpenGL REQUIRED)

project(Template)
# TODO: needed for clap-wrapper, who should really be doing this for us
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(${PROJECT_NAME} MODULE 
	src/plugin.cpp
	src/gui.cpp
	src/audio.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC
	include
	PRIVATE
	src
)

target_link_libraries(${PROJECT_NAME} PRIVATE KitDSP clap etl SDL3 imgui imgui_impl OpenGL::OpenGL OpenGL::EGL)

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".clap")

add_library(${PROJECT_NAME}_vst3 MODULE)
set_property(TARGET ${PROJECT_NAME}_vst3 PROPERTY POSITION_INDEPENDENT_CODE ON)
target_add_vst3_wrapper(TARGET ${PROJECT_NAME}_vst3
	OUTPUT_NAME "${PROJECT_NAME}"
	SUPPORTS_ALL_NOTE_EXPRESSIONS TRUE
)

add_library(imgui INTERFACE
	${IMGUI_DIR}/imgui.cpp
	${IMGUI_DIR}/imgui_demo.cpp
	${IMGUI_DIR}/imgui_draw.cpp
	${IMGUI_DIR}/imgui_tables.cpp
	${IMGUI_DIR}/imgui_widgets.cpp
)
target_include_directories(imgui INTERFACE ${IMGUI_DIR})
add_library(imgui_impl INTERFACE
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
)
target_include_directories(imgui_impl INTERFACE ${IMGUI_DIR}/backends)
target_link_libraries(imgui_impl INTERFACE imgui)
