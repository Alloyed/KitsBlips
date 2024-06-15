cmake_minimum_required(VERSION 3.26)
cmake_policy(SET CMP0048 NEW)

project(kitblips_all)

option(ENABLE_JUCE "build juce plugins" ON)
option(ENABLE_VCVRACK "build vcv rack plugins" ON)
option(ENABLE_DAISY "build daisy firwmare" OFF)

# platform-independent libraries
add_subdirectory(sdk/DaisySP)
add_subdirectory(dsp)

if(${ENABLE_JUCE})
    add_subdirectory(sdk/JUCE)
    add_subdirectory(juce/ExamplePlugin)
endif()

if(${ENABLE_VCVRACK})
    add_subdirectory(vcvrack)
endif()

if(${ENABLE_DAISY})
    add_subdirectory(sdk/libDaisy)
    add_subdirectory(daisy/ExampleFirmware)
endif()