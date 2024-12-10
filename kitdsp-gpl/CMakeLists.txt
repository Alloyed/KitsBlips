cmake_minimum_required(VERSION 3.26)
project(KitDSP_GPL)

add_subdirectory(../kitdsp kitdsp)

add_library(KitDSP_GPL INTERFACE)

# C++11 is required for ABI compatibility with VCV Rack 2, which has standardized on an old version of mingw
set_target_properties(KitDSP_GPL PROPERTIES PUBLIC
	CXX_STANDARD 11
	CXX_STANDARD_REQUIRED
)

target_include_directories(KitDSP_GPL INTERFACE
	include
)

target_link_libraries(KitDSP_GPL INTERFACE
    KitDSP
)

# when built standalone, enable tests
if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
	file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/CTestCustom.cmake
	"set(CTEST_CUSTOM_POST_TEST \"${CMAKE_CURRENT_SOURCE_DIR}../kitdsp/test/spectrograms.sh\")")

	include(CTest)
	add_subdirectory(test)
endif()