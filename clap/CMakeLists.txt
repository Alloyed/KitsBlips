cmake_minimum_required(VERSION 3.22)
cmake_policy(SET CMP0048 NEW)

# TODO: needed for clap-wrapper, who should really be doing this for us
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15")
    enable_language(OBJC OBJCXX)
endif()
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(KitsBlips VERSION 0.0.0)
set(PRODUCT_NAME "KitsBlips")
set(PRODUCT_ID "me.alloyed.KitsBlips")
# VSTs require <= 17 for codecvt, and CLAP requires >= 17 for std::filesystem :/
set(CMAKE_CXX_STANDARD 17)

# shared libs
include(../content.cmake)
include(cmake/imgui.cmake)
set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES TRUE CACHE BOOL "Download Dependencies")
FetchContent_MakeAvailable(clap clap-wrapper SDL3 KitDSP)

add_library(${PROJECT_NAME}-plugin STATIC
    src/entry.cpp

    src/clapApi/basePlugin.cpp
    src/clapApi/effectPlugin.cpp
    src/clapApi/pluginHost.cpp

    #src/gui/sdlImgui.cpp

    src/template/plugin.cpp
    src/snecho/plugin.cpp
)

target_include_directories(${PROJECT_NAME}-plugin
    PUBLIC
    src
)

target_link_libraries(${PROJECT_NAME}-plugin
    PUBLIC
    clap
    KitDSP
    etl
    #imgui
    #imgui_opengl_sdl3
    PRIVATE
)

target_compile_definitions(${PROJECT_NAME}-plugin PUBLIC
    PRODUCT_NAME="${PRODUCT_NAME}"
    PRODUCT_ID="${PRODUCT_ID}"
    PRODUCT_VERSION="${PROJECT_VERSION}"
)

make_clapfirst_plugins(
    TARGET_NAME ${PROJECT_NAME}
    IMPL_TARGET ${PROJECT_NAME}-plugin

    OUTPUT_NAME "${PRODUCT_NAME}"

    ENTRY_SOURCE src/entry.cpp

    BUNDLE_IDENTIFER "${PRODUCT_ID}"
    BUNDLE_VERSION ${PROJECT_VERSION}

    COPY_AFTER_BUILD FALSE

    PLUGIN_FORMATS CLAP VST3 # AUV2 WCLAP

    ASSET_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PROJECT_NAME}_assets

    # You can set up the AUv2 for a single plugin here, or you can
    # set it up with the auv2 extension in your clap
    # AUV2_MANUFACTURER_NAME "Free Audio"
    # AUV2_MANUFACTURER_CODE "FrAD"
    # AUV2_SUBTYPE_CODE "BdDt"
    # AUV2_INSTRUMENT_TYPE "aufx"

    # You can add a target-per-standalone you want. Syntax here is
    #   target-postfix output-name clap-id
    # This allows you to make multiple standalones from a multi-plugin clap
    # In this case, the standalone has no UI and is an audio to audio effect
    # so it isn't very useful
    STANDALONE_CONFIGURATIONS
    standalone "${PRODUCT_NAME}" "${PRODUCT_ID}"
)
