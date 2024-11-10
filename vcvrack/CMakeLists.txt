cmake_minimum_required(VERSION 3.16)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9" CACHE STRING "Minimum OS X deployment version")
project(KitsBlips_VCV)

set(PLUGIN_NAME ${PROJECT_NAME})
set(ADDITIONAL_PLUGIN_DISTRIBUTABLES README.md)
include(RackSDK.cmake)

add_subdirectory(../sdk/DaisySP dasiysp)
add_subdirectory(../kitdsp kitdsp)

target_sources(${RACK_PLUGIN_LIB} PRIVATE
    src/plugin.cpp
    src/snecho.cpp
    src/psxverb.cpp
    src/dance.cpp
    src/dsf.cpp
)

target_link_libraries(${RACK_PLUGIN_LIB} PRIVATE
    KitDSP
    DaisySP
)