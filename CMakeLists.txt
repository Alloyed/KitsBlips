cmake_minimum_required(VERSION 3.26)
cmake_policy(SET CMP0048 NEW)

project(kitblips_all)

# SDK paths

# DaisySP (git submodule
add_subdirectory(sdk/DaisySP)

# # libDaisy (fetch)
# include(FetchContent)
# FetchContent_Declare(daisy
#   GIT_REPOSITORY https://github.com/stellar-aria/libDaisy
#   GIT_TAG cmake-only
# )
# FetchContent_MakeAvailable(daisy)

# juce (git submodule)
add_subdirectory(sdk/JUCE)

# project paths
add_subdirectory(dsp)
add_subdirectory(vcvrack)
add_subdirectory(juce/ExamplePlugin)
# add_subdirectory(daisy/ExampleFirmware)

