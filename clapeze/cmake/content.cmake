include(FetchContent)

function(target_enable_warnings target_name)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-braces -Wno-nested-anon-types -Wno-gnu-anonymous-struct -Wno-c++98-compat -Wno-c++98-compat-pedantic)
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-braces)
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${target_name} PRIVATE /W4)
    endif()
endfunction()

function(target_disable_warnings target_name)
    get_target_property(include_dirs ${target_name} INTERFACE_INCLUDE_DIRECTORIES)
    set_target_properties(${target_NAME} PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${include_dirs}")
endfunction()

function(enable_ccache)
    find_program(CCACHE_PROGRAM ccache)
    if(CCACHE_PROGRAM)
        set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
        set(CMAKE_CUDA_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    endif()
endfunction()

# remote
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt
    GIT_TAG        11.2.0
    SYSTEM
    EXCLUDE_FROM_ALL
)
FetchContent_Declare(
    etl
    GIT_REPOSITORY https://github.com/ETLCPP/etl
    GIT_TAG        20.44.2
    SYSTEM
    EXCLUDE_FROM_ALL
)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.17.0
    SYSTEM
    EXCLUDE_FROM_ALL
)
FetchContent_Declare(
    clap
    GIT_REPOSITORY https://github.com/free-audio/clap.git
    GIT_TAG        1.2.7
    SYSTEM
    EXCLUDE_FROM_ALL
)
FetchContent_Declare(
	miniz
	GIT_REPOSITORY https://github.com/richgel999/miniz.git
	GIT_TAG        3.1.0
    SYSTEM
    EXCLUDE_FROM_ALL
)
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
    SYSTEM
    EXCLUDE_FROM_ALL
)
