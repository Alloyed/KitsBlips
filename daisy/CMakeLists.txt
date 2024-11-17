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
FetchContent_Declare(KitDSP
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../kitdsp
)

project(KitsBlips_daisy)

FetchContent_MakeAvailable(daisy)
FetchContent_MakeAvailable(DaisySP)
FetchContent_MakeAvailable(KitDSP)

# Common firmware options
set(FIRMWARE_RELEASE_OPT_LEVEL -Os)

# add each new firmware here
add_subdirectory(calibrate)
add_subdirectory(follow)
add_subdirectory(psxverb)
add_subdirectory(quantize)
add_subdirectory(snecho)
add_subdirectory(sum)
add_subdirectory(template)