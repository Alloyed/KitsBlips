cmake_minimum_required(VERSION 3.26)
project(KitDSP)

add_library(KitDSP STATIC
	src/psxReverb.cpp
	src/snesEcho.cpp
	src/dsfOscillator.cpp
)

# C++11 is required for ABI compatibility with VCV Rack 2, which has standardized on an old version of mingw
set_target_properties(KitDSP PROPERTIES PUBLIC
	CXX_STANDARD 11
	CXX_STANDARD_REQUIRED
)

target_include_directories(KitDSP PUBLIC
	include
	PRIVATE
	src
)

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_definitions(KitDSP INTERFACE _USE_MATH_DEFINES)
endif()

# when built standalone, enable tests
if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
	include(CTest)
	add_subdirectory(test)
endif()