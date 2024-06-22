cmake_minimum_required(VERSION 3.22)
cmake_policy(SET CMP0048 NEW)

project(kitblips_all)

option(ENABLE_JUCE "build juce plugins" OFF)
option(ENABLE_VCVRACK "build vcv rack plugins" OFF)
option(ENABLE_DAISY "build daisy firwmare" OFF)

# platform-independent libraries
add_subdirectory(dsp)

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    # https://stackoverflow.com/questions/6563810/m-pi-works-with-math-h-but-not-with-cmath-in-visual-studio
    target_compile_definitions(KitDSP PUBLIC _USE_MATH_DEFINES)
    #target_compile_definitions(DaisySP PUBLIC _USE_MATH_DEFINES)
else()
    # doesn't run on msvc yet: https://github.com/electro-smith/DaisySP/pull/197
    add_subdirectory(sdk/DaisySP)
endif()

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