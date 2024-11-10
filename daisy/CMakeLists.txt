cmake_minimum_required(VERSION 3.26)
cmake_policy(SET CMP0048 NEW)

include(FetchContent)

#deps
FetchContent_Declare(daisy
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/../sdk/libDaisy
)
FetchContent_Declare(DaisySP
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/../sdk/DaisySP
)
FetchContent_Declare(kitdsp
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/../dsp
)

project(KitsBlips_daisy)

FetchContent_MakeAvailable(daisy)
FetchContent_MakeAvailable(DaisySP)
FetchContent_MakeAvailable(kitdsp)

add_subdirectory(ExampleFirmware)
