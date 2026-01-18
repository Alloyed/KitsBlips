cmake_minimum_required(VERSION 3.22)
cmake_policy(SET CMP0048 NEW)
project(KitsBlips VERSION 0.0.0)

set(PRODUCT_NAME "KitsBlips")
set(PRODUCT_ID "me.alloyed.KitsBlips")

include(CMakeDependentOption)
option(KITSBLIPS_ENABLE_GUI "enable GUI" ON)
option(KITSBLIPS_ENABLE_VST "build VSTs" ON)
option(KITSBLIPS_ENABLE_SENTRY "Enable sentry error reporting" OFF)
if(APPLE)
    option(KITSBLIPS_ENABLE_AUDIOUNIT "build Audio Units" OFF)
endif()

# TODO: needed for clap-wrapper, who should really be doing this for us
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15")
    enable_language(OBJC OBJCXX)
endif()
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
#end globals
set(CMAKE_CXX_STANDARD 20)

# shared libs
include(../content.cmake)
set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES TRUE CACHE BOOL "Download Dependencies")
FetchContent_MakeAvailable(clap fmt clapeze KitDSP)
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
    src/gui/kitguiFeature.cpp
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

if(KITSBLIPS_ENABLE_SENTRY)
    # TODO: move into clapeze?
    FetchContent_MakeAvailable(sentry-native)
    if(NOT DEFINED ENV{KITSBLIPS_SENTRY_DSN})
        message(FATAL_ERROR "Sentry requires the environment variable KITSBLIPS_SENTRY_DSN to be defined")
    endif()
    target_compile_definitions(KitsBlips PUBLIC
        KITSBLIPS_ENABLE_SENTRY=1
        KITSBLIPS_SENTRY_DSN="$ENV{KITSBLIPS_SENTRY_DSN}"
    )
    target_link_libraries(KitsBlips PRIVATE sentry)
    # dummy target used to re-add the handler to the default build without pulling in the rest of sentry
    add_custom_target(handler-dist ALL
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:crashpad_handler>"  "${CMAKE_CURRENT_BINARY_DIR}/dist"
    )
    add_dependencies(handler-dist crashpad_handler)
endif()


# as shrimple as that!
add_library(KitsBlips-clap MODULE)
target_link_libraries(KitsBlips-clap PUBLIC KitsBlips)
target_add_clap_configuration(TARGET KitsBlips-clap
    OUTPUT_NAME "KitsBlips"
    BUNDLE_IDENTIFIER "${PRODUCT_ID}"
    BUNDLE_VERSION "${PROJECT_VERSION}"
)
target_add_bundle(KitsBlips-clap "${CMAKE_CURRENT_LIST_DIR}/assets" "${CMAKE_CURRENT_BINARY_DIR}/dist/KitsBlips.clap")

if(KITSBLIPS_ENABLE_VST)
    add_library(KitsBlips-vst3 MODULE)
    target_link_libraries(KitsBlips-vst3 PRIVATE KitsBlips)
    target_add_vst3_wrapper(TARGET KitsBlips-vst3 
        OUTPUT_NAME "KitsBlips"
        BUNDLE_IDENTIFIER "${PRODUCT_ID}"
        BUNDLE_VERSION "${PROJECT_VERSION}"
        SUPPORTS_ALL_NOTE_EXPRESSIONS TRUE
        ASSET_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tmp" # default doesn't work on mac
        WINDOWS_FOLDER_VST3 FALSE
    )
    target_add_bundle(KitsBlips-vst3 "${CMAKE_CURRENT_LIST_DIR}/assets" "${CMAKE_CURRENT_BINARY_DIR}/dist/KitsBlips.vst3")
endif()

if(KITSBLIPS_ENABLE_AUDIOUNIT)
    # TODO
    add_library(KitsBlips-au MODULE)
    target_link_libraries(KitsBlips-au PRIVATE KitsBlips)
    target_add_auv2_wrapper(TARGET KitsBlips-au
        OUTPUT_NAME "KitsBlips"
        BUNDLE_IDENTIFIER "${PRODUCT_ID}"
        BUNDLE_VERSION "${PROJECT_VERSION}"

        # MANUFACTURER_NAME
        # MANUFACTURER_CODE
        # SUBTYPE_CODE
        # INSTRUMENT_TYPE
    )
    # the normal vst wrapper includes a $<CONFIG> generator in its output path, which add_bundle() can't support for dumb cmake reasons :/
    set_target_properties(KitsBlips-au PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tmp" )
    target_add_bundle(KitsBlips-au "${CMAKE_CURRENT_LIST_DIR}/assets")
endif()

# when built standalone, enable tests
if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    include(CTest)
    add_subdirectory(test)
endif()

