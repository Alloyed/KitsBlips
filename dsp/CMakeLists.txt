add_library(KitDSP STATIC src/psxReverb.cpp)

set_target_properties(KitDSP PROPERTIES PUBLIC
	CXX_STANDARD 14
	CXX_STANDARD_REQUIRED
	)


target_include_directories(KitDSP PUBLIC
	${CMAKE_CURRENT_LIST_DIR}/include
	PRIVATE
	"src"
	)
