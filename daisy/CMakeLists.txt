cmake_minimum_required(VERSION 3.26)
cmake_policy(SET CMP0048 NEW)

# Use Daisy's toolchain
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    include(${CMAKE_CURRENT_LIST_DIR}/../sdk/libDaisy/cmake/toolchains/autodetect.cmake)
endif()

include(FetchContent)

#deps
FetchContent_Declare(daisy
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../sdk/libDaisy
)
FetchContent_Declare(DaisySP
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../sdk/DaisySP
)
FetchContent_Declare(kitdsp
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../dsp
)

project(KitsBlips_daisy)

FetchContent_MakeAvailable(daisy)
FetchContent_MakeAvailable(DaisySP)
FetchContent_MakeAvailable(kitdsp)

add_subdirectory(ExampleFirmware)
