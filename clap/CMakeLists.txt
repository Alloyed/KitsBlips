cmake_minimum_required(VERSION 3.22)
cmake_policy(SET CMP0048 NEW)

# TODO: needed for clap-wrapper, who should really be doing this for us
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15")
    enable_language(OBJC OBJCXX)
endif()
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
#end globals

project(KitsBlips VERSION 0.0.0)
set(PRODUCT_NAME "KitsBlips")
set(PRODUCT_ID "me.alloyed.KitsBlips")
option(KITSBLIPS_ENABLE_GUI "enable GUI" ON)

# VSTs use codecvt in their headers, which are removed in C++26.
# ETL expects char8_t to be a keyword post C++20, but clap-wrapper hard-disables it. so we'll target our plugin narrowly for C++23
set(CMAKE_CXX_STANDARD 17)

# shared libs
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(../content.cmake)
set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES TRUE CACHE BOOL "Download Dependencies")
FetchContent_MakeAvailable(clap clap-wrapper KitDSP)
find_package(Sanitizers)

add_library(KitsBlips-lib STATIC)
set_property(TARGET KitsBlips-lib PROPERTY CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_compile_definitions(KitsBlips-lib PUBLIC
    PRODUCT_NAME="${PRODUCT_NAME}"
    PRODUCT_ID="${PRODUCT_ID}"
    PRODUCT_VERSION="${PROJECT_VERSION}"
)
if (MSVC)
    target_compile_options(KitsBlips-lib PRIVATE /W4)
else()
    target_compile_options(KitsBlips-lib PRIVATE -Wall -Wextra -pedantic -Wno-unused-parameter)
endif()

target_sources(KitsBlips-lib PRIVATE
    src/entry.cpp

    src/clapeze/basePlugin.cpp
    src/clapeze/pluginHost.cpp

    src/crunch/crunch.cpp
    src/sines/sines.cpp
    src/snecho/snecho.cpp
)

target_include_directories(KitsBlips-lib PUBLIC src)

target_link_libraries(KitsBlips-lib PUBLIC
    clap
    KitDSP
    etl
)

if(KITSBLIPS_ENABLE_GUI)
    include(imgui)
    set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES TRUE CACHE BOOL "Download Dependencies")
    FetchContent_MakeAvailable(SDL3 glm tinygltf)

    target_compile_definitions(KitsBlips-lib PUBLIC KITSBLIPS_ENABLE_GUI=1)
    target_sources(KitsBlips-lib PRIVATE
        src/clapeze/gui/sdlOpenGlExt.cpp
        src/clapeze/gui/imguiExt.cpp
        src/gui/knob.cpp
        src/gui/mesh.cpp
    )
    target_link_libraries(KitsBlips-lib PUBLIC
        imgui
        imgui_opengl_sdl3
        glm
        tinygltf
    )
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        target_sources(KitsBlips-lib PUBLIC
            src/clapeze/gui/platform/platformWin32.cpp
        )
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        target_sources(KitsBlips-lib PUBLIC
            src/clapeze/gui/platform/platformCocoa.cpp
            src/clapeze/gui/platform/platformCocoa.m
        )
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(X11)
        target_sources(KitsBlips-lib PUBLIC
            src/clapeze/gui/platform/platformLinux.cpp
        )
        target_link_libraries(KitsBlips-lib PUBLIC X11::X11 X11::Xutil)
    endif()
endif()

# bleh
# Create a .dSYM file on macOS
if(APPLE)
  add_custom_command(
    TARGET your_target
    POST_BUILD
    COMMAND dsymutil $<TARGET_FILE:KitsBlips-lib>
  )
endif()

make_clapfirst_plugins(
    TARGET_NAME KitsBlips
    IMPL_TARGET KitsBlips-lib

    OUTPUT_NAME "${PRODUCT_NAME}"

    ENTRY_SOURCE src/entry.cpp

    BUNDLE_IDENTIFER "${PRODUCT_ID}"
    BUNDLE_VERSION ${PROJECT_VERSION}

    COPY_AFTER_BUILD FALSE

    PLUGIN_FORMATS CLAP VST3 # AUV2 WCLAP

    ASSET_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/dist

    # You can set up the AUv2 for a single plugin here, or you can
    # set it up with the auv2 extension in your clap
    # AUV2_MANUFACTURER_NAME "Free Audio"
    # AUV2_MANUFACTURER_CODE "FrAD"
    # AUV2_SUBTYPE_CODE "BdDt"
    # AUV2_INSTRUMENT_TYPE "aufx"

    # You can add a target-per-standalone you want. Syntax here is
    #   target-postfix output-name clap-id
    STANDALONE_CONFIGURATIONS
    sines "sines" "kitsblips.sines"
)
