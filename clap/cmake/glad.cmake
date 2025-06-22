find_package(OpenGL REQUIRED)

add_library(glad STATIC EXCLUDE_FROM_ALL
	${GLAD_DIR}/src/gl.c
)
target_include_directories(glad INTERFACE ${GLAD_DIR}/include)
target_include_directories(glad PRIVATE ${GLAD_DIR}/include)
