add_library(pugl STATIC EXCLUDE_FROM_ALL
	${PUGL_DIR}/src/common.c
	${PUGL_DIR}/src/internal.c
)
target_include_directories(pugl PUBLIC ${PUGL_DIR}/include)
target_include_directories(pugl PRIVATE ${PUGL_DIR}/src)

if(KITGUI_USE_X11)
	add_library(pugl_x11 STATIC EXCLUDE_FROM_ALL
		${PUGL_DIR}/src/x11.c
		${PUGL_DIR}/src/x11_gl.c
	)
	target_link_libraries(pugl_x11 PUBLIC OpenGL::GL)
elseif(KITGUI_USE_WIN32)
	add_library(pugl_win32 STATIC EXCLUDE_FROM_ALL
		${PUGL_DIR}/src/win.c
		${PUGL_DIR}/src/win_gl.c
	)
	target_link_libraries(pugl_win32 PUBLIC OpenGL::GL)
elseif(KITGUI_USE_COCOA)
	add_library(pugl_cocoa STATIC EXCLUDE_FROM_ALL
		${PUGL_DIR}/src/mac.m
		${PUGL_DIR}/src/mac_gl.m
	)
	target_link_libraries(pugl_cocoa PUBLIC OpenGL::GL)
endif()