cmake_minimum_required(VERSION 3.22)
cmake_policy(SET CMP0048 NEW)
project(KitsBlips_JUCE)

# shared libs
add_subdirectory(../sdk/JUCE juce)
add_subdirectory(../dsp kitdsp)

# plugins
add_subdirectory(ExamplePlugin)
