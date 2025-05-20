cmake_minimum_required(VERSION 3.22)
cmake_policy(SET CMP0048 NEW)
project(KitsClaps)

# shared libs
set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES TRUE CACHE BOOL "Download Dependencies")
include(../content.cmake)
FetchContent_MakeAvailable(clap clap-wrapper SDL3 KitDSP)

add_subdirectory(template)
