cmake_minimum_required(VERSION 3.26)
project(KitDSP)

include(../content.cmake)
FetchContent_MakeAvailable(etl)
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "ARM")
	FetchContent_MakeAvailable(CMSIS-DSP)
endif()

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

target_link_libraries(KitDSP PUBLIC etl::etl)

# when built standalone, enable tests
if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
	file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/CTestCustom.cmake
	"set(CTEST_CUSTOM_POST_TEST \"${CMAKE_CURRENT_SOURCE_DIR}/test/spectrograms.sh\")")

	include(CTest)
	add_subdirectory(test)
endif()