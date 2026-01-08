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
option(KITSBLIPS_ENABLE_VST "build VSTs" ON)

set(CMAKE_CXX_STANDARD 20)

# shared libs
include(../content.cmake)
include(cmake/add_bundle.cmake)
set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES TRUE CACHE BOOL "Download Dependencies")
FetchContent_MakeAvailable(clap fmt clapeze KitDSP physfs)
FetchContent_MakeAvailable(clap-wrapper)

add_library(KitsBlips OBJECT)
target_compile_definitions(KitsBlips PUBLIC
    PRODUCT_NAME="${PRODUCT_NAME}"
    PRODUCT_ID="${PRODUCT_ID}"
    PRODUCT_VERSION="${PROJECT_VERSION}"
)
target_enable_warnings(KitsBlips)

target_sources(KitsBlips PRIVATE
    src/entry.cpp
    src/distort/distort.cpp
    src/snecho/snecho.cpp
    src/chorus/chorus.cpp
    src/freqshift/freqshift.cpp
    src/harmonizer/harmonizer.cpp
    src/keysynth/keysynth.cpp
    src/gui/feature.cpp
)

target_include_directories(KitsBlips PUBLIC src)

target_link_libraries(KitsBlips PRIVATE
    clapeze
    KitDSP
)

if(KITSBLIPS_ENABLE_GUI)
    FetchContent_MakeAvailable(kitgui)

    target_compile_definitions(KitsBlips PUBLIC KITSBLIPS_ENABLE_GUI=1)
    target_link_libraries(KitsBlips PRIVATE kitgui)
endif()

# as shrimple as that!
add_library(KitsBlips-clap MODULE)
target_link_libraries(KitsBlips-clap PUBLIC KitsBlips)
add_bundle(KitsBlips-clap "${CMAKE_CURRENT_BINARY_DIR}/dist/KitsBlips.clap" "${CMAKE_CURRENT_LIST_DIR}/assets")

if(KITSBLIPS_ENABLE_VST)
    add_library(KitsBlips-vst3 MODULE)
    target_link_libraries(KitsBlips-vst3 PRIVATE KitsBlips)
    target_add_vst3_wrapper(TARGET KitsBlips-vst3 
        OUTPUT_NAME "KitsBlips"
        BUNDLE_IDENTIFIER "${PRODUCT_ID}"
        BUNDLE_VERSION "${PROJECT_VERSION}"
        SUPPORTS_ALL_NOTE_EXPRESSIONS TRUE
    )
    add_bundle(KitsBlips-vst3 "${CMAKE_CURRENT_BINARY_DIR}/dist/KitsBlips.vst3" "${CMAKE_CURRENT_LIST_DIR}/assets")
endif()

# when built standalone, enable tests
if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    include(CTest)
    add_subdirectory(test)
endif()

