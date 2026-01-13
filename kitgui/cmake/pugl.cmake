add_library(pugl STATIC EXCLUDE_FROM_ALL
	${PUGL_DIR}/src/common.c
	${PUGL_DIR}/src/internal.c
)
target_include_directories(pugl PUBLIC ${PUGL_DIR}/include)
target_include_directories(pugl PRIVATE ${PUGL_DIR}/src)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	target_sources(pugl PRIVATE
		${PUGL_DIR}/src/win.c
		${PUGL_DIR}/src/win_gl.c
	)
	target_link_libraries(pugl PUBLIC OpenGL::GL)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	target_sources(pugl PRIVATE
		${PUGL_DIR}/src/mac.m
		${PUGL_DIR}/src/mac_gl.m
	)
	target_link_libraries(pugl PUBLIC OpenGL::GL)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(X11)
	target_sources(pugl PRIVATE
		${PUGL_DIR}/src/x11.c
		${PUGL_DIR}/src/x11_gl.c
	)
	target_link_libraries(pugl PUBLIC
		OpenGL::GL
        X11::X11
        X11::Xutil
        X11::Xcursor
        X11::Xrandr
        X11::Xext
	)
endif()