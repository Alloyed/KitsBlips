cmake_minimum_required(VERSION 3.22)
cmake_policy(SET CMP0048 NEW)
project(clapeze VERSION 0.0.0)

include(../content.cmake)
FetchContent_MakeAvailable(clap etl physfs)

add_library(clapeze STATIC)
set_target_properties(clapeze PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_features(clapeze PRIVATE cxx_std_20)
target_enable_warnings(clapeze)

target_sources(clapeze PRIVATE
    src/basePlugin.cpp
    src/pluginHost.cpp
    src/ext/parameterConfigs.cpp
)

target_include_directories(clapeze PUBLIC include)

target_link_libraries(clapeze PUBLIC
    clap
    etl
    PhysFS::PhysFS-static
)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    set(CMAKE_CXX_VISIBILITY_PRESET "hidden")
    set(CMAKE_VISIBILITY_INLINES_HIDDEN TRUE)

    include(GenerateExportHeader)

    add_library(clapeze-examples MODULE)
    set_target_properties(clapeze-examples PROPERTIES SUFFIX ".clap" PREFIX "")
    target_enable_warnings(clapeze-examples)
    generate_export_header(clapeze-examples BASE_NAME clapeze EXPORT_FILE_NAME "${CMAKE_CURRENT_LIST_DIR}/examples/clapeze_export_generated.h")

    target_sources(clapeze-examples PRIVATE
        examples/entry.cpp
        examples/sines/sines.cpp
        examples/crunch/crunch.cpp
    )

    target_include_directories(clapeze-examples PRIVATE examples)

    target_link_libraries(clapeze-examples PUBLIC clapeze)
endif()
