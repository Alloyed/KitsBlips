find_package(OpenGL REQUIRED)

project(Template VERSION 0.0.0)
set(PRODUCT_NAME "Template")
set(PRODUCT_ID "KitsBlips.Template")

# TODO: needed for clap-wrapper, who should really be doing this for us
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 17)

add_library(${PROJECT_NAME}-impl STATIC 
	src/plugin.cpp
	src/PluginHost.cpp
	src/gui.cpp
	src/audio.cpp
)

target_include_directories(${PROJECT_NAME}-impl PUBLIC
	include
	PRIVATE
	src
)

target_link_libraries(${PROJECT_NAME}-impl PUBLIC
	SDL3::SDL3-static
	KitDSP
	clap
	etl
	imgui
	imgui_impl
	OpenGL::GL
)

target_compile_definitions(${PROJECT_NAME}-impl PUBLIC
	PRODUCT_NAME="${PRODUCT_NAME}"
	PRODUCT_ID="${PRODUCT_ID}"
	PRODUCT_VERSION="${PROJECT_VERSION}"
)

make_clapfirst_plugins(
        TARGET_NAME ${PROJECT_NAME}
        IMPL_TARGET ${PROJECT_NAME}-impl

        OUTPUT_NAME "${PRODUCT_NAME}"

        ENTRY_SOURCE src/plugin.cpp

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
