cmake_minimum_required(VERSION 3.22)
cmake_policy(SET CMP0048 NEW)
project(clapeze VERSION 0.0.0)

include(cmake/content.cmake)
FetchContent_MakeAvailable(clap etl miniz fmt tomlplusplus)

add_library(clapeze STATIC)
set_target_properties(clapeze PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(fmt PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(miniz PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_features(clapeze PRIVATE cxx_std_20)
target_enable_warnings(clapeze)
target_disable_warnings(miniz)

target_sources(clapeze PRIVATE
    src/basePlugin.cpp
    src/pluginHost.cpp
    src/entryPoint.cpp
    src/ext/assets.cpp
    src/ext/presets.cpp
    src/params/dynamicParametersFeature.cpp
    src/params/enumParametersFeature.cpp
    src/params/parameterTypes.cpp
    src/state/tomlImplementation.cpp
)

target_include_directories(clapeze PUBLIC include)
target_compile_definitions(clapeze PUBLIC
    TOML_HEADER_ONLY=0
    TOML_EXCEPTIONS=0
)

target_link_libraries(clapeze PUBLIC
    clap
    etl
    miniz
    fmt::fmt
    tomlplusplus::tomlplusplus
)

include(cmake/target_add_bundle.cmake)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    set(CMAKE_CXX_VISIBILITY_PRESET "hidden")
    set(CMAKE_VISIBILITY_INLINES_HIDDEN TRUE)

    add_library(clapeze-examples MODULE)
    set_target_properties(clapeze-examples PROPERTIES SUFFIX ".clap" PREFIX "")
    target_enable_warnings(clapeze-examples)

    target_sources(clapeze-examples PRIVATE
        examples/entry.cpp
        examples/instrumentExample.cpp
        examples/effectExample.cpp
    )

    target_include_directories(clapeze-examples PRIVATE examples)

    target_link_libraries(clapeze-examples PUBLIC clapeze)

	include(CTest)
	add_subdirectory(test)
endif()
