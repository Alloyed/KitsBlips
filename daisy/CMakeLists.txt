cmake_minimum_required(VERSION 3.26)
cmake_policy(SET CMP0048 NEW)

# Use Daisy's toolchain
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    include(cmake/toolchains/autodetect.cmake)
endif()

include(../content.cmake)

project(KitsBlips_daisy)

FetchContent_MakeAvailable(daisy)
FetchContent_MakeAvailable(KitDSP)

# Common firmware options
set(FIRMWARE_RELEASE_OPT_LEVEL -Os)

# add each new firmware here
add_subdirectory(calibrate)
add_subdirectory(crunch)
add_subdirectory(follow)
add_subdirectory(psxverb)
add_subdirectory(quantize)
add_subdirectory(snecho)
add_subdirectory(sum)
add_subdirectory(template)
add_subdirectory(adsr)
add_subdirectory(fourstep)
add_subdirectory(vca)
add_subdirectory(vco)
