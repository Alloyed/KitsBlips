cmake_minimum_required(VERSION 3.16)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9" CACHE STRING "Minimum OS X deployment version")
project(KitsBlips_VCV)

if(WIN32)
    set(RACK_SDK_DIR ${CMAKE_CURRENT_LIST_DIR}/../sdk/Rack-SDK-2.5.2-win-x64 CACHE PATH "Location of VCV rack SDK")
elseif(LINUX)
    set(RACK_SDK_DIR ${CMAKE_CURRENT_LIST_DIR}/../sdk/Rack-SDK-2.5.2-lin-x64 CACHE PATH "Location of VCV rack SDK")
else()
    set(RACK_SDK_DIR "" CACHE PATH "Location of VCV rack SDK")
endif()

set(PLUGIN_NAME ${PROJECT_NAME})
set(ADDITIONAL_PLUGIN_DISTRIBUTABLES README.md)
include(RackSDK.cmake)

add_subdirectory(../dsp kitdsp)

target_sources(${RACK_PLUGIN_LIB} PRIVATE
    src/plugin.cpp
    src/snecho.cpp
    src/psxverb.cpp
    src/dance.cpp
    src/dsf.cpp
)

target_link_libraries(${RACK_PLUGIN_LIB} PRIVATE
    KitDSP
)