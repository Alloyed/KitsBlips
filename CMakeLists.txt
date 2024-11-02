cmake_minimum_required(VERSION 3.22)
cmake_policy(SET CMP0048 NEW)
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/dist")

project(kitblips_all)

include(ExternalProject)

option(ENABLE_JUCE "build juce plugins" ON)
option(ENABLE_VCVRACK "build vcv rack plugins" ON)
option(ENABLE_DAISY "build daisy firwmare" OFF)
set(RACK_SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/sdk/Rack-SDK-2.5.2-win-x64 CACHE PATH "Location of VCV rack SDK")


if(${ENABLE_JUCE})
    set(JUCE_GENERATOR "Unix Makefiles")
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(JUCE_GENERATOR "Visual Studio 17 2022")
    endif()

    ExternalProject_Add(
        KitBlips_Juce
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/juce
	CMAKE_CACHE_ARGS -DCMAKE_INSTALL_PREFIX=:PATH=${CMAKE_INSTALL_PREFIX}
        CMAKE_GENERATOR ${JUCE_GENERATOR}
	#INSTALL_COMMAND ""
    )
endif()

if(${ENABLE_VCVRACK})
    set(RACK_GENERATOR "Unix Makefiles")
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(RACK_GENERATOR "MSYS Makefiles")
    endif()

    ExternalProject_Add(
        KitBlips_Rack
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vcvrack
	CMAKE_CACHE_ARGS -DRACK_SDK_DIR:STRING=${RACK_SDK_DIR} -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}
        CMAKE_GENERATOR ${RACK_GENERATOR}
	#INSTALL_COMMAND ""
    )
endif()

#
#if(${ENABLE_DAISY})
#    add_subdirectory(sdk/libDaisy)
#    add_subdirectory(daisy/ExampleFirmware)
#endif()
