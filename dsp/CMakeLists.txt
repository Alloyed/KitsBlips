add_library(KitDSP STATIC
	src/psxReverb.cpp
	src/resampler.cpp
	src/snesEcho.cpp
	src/dsfOscillator.cpp
)

set_target_properties(KitDSP PROPERTIES PUBLIC
	CXX_STANDARD 11
	CXX_STANDARD_REQUIRED
)

target_include_directories(KitDSP PUBLIC
	${CMAKE_CURRENT_LIST_DIR}/include
	PRIVATE
	"src"
	)

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_definitions(KitDSP INTERFACE _USE_MATH_DEFINES)
endif()
