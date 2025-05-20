project(Template)
# TODO: needed for clap-wrapper, who should really be doing this for us
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(${PROJECT_NAME} MODULE 
	src/plugin.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC
	include
	PRIVATE
	src
)

target_link_libraries(${PROJECT_NAME} PRIVATE KitDSP clap)

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".clap")

add_library(${PROJECT_NAME}_vst3 MODULE)
set_property(TARGET ${PROJECT_NAME}_vst3 PROPERTY POSITION_INDEPENDENT_CODE ON)
target_add_vst3_wrapper(TARGET ${PROJECT_NAME}_vst3
	OUTPUT_NAME "${PROJECT_NAME}"
	SUPPORTS_ALL_NOTE_EXPRESSIONS TRUE
)
