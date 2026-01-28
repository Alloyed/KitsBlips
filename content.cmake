# for clap-wrapper
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15")
    enable_language(OBJC OBJCXX)
endif()

# with this flag we can use set() to set options for the following packages
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

# needed for any dynamic libs on linux
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CXX_VISIBILITY_PRESET "hidden")

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
    set_target_properties(${target_name} PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${include_dirs}")
endfunction()

function(enable_asan)
    # https://clang.llvm.org/docs/AddressSanitizer.html
    if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "AddressSanitizer enabled, so we need to use clang.")
    endif()
    set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -O1 -g -fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls)
endfunction()

function(enable_msan)
    # https://clang.llvm.org/docs/MemorySanitizer.html
    if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "MemorySanitizer enabled, so we need to use clang.")
    endif()
    set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -O1 -g -fsanitize=memory -fsanitize-memory-track-origins -fno-omit-frame-pointer -fno-optimize-sibling-calls)
endfunction()

function(enable_tsan)
    # https://clang.llvm.org/docs/ThreadSanitizer.html
    if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "ThreadSanitizer enabled, so we need to use clang.")
    endif()
    set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -O1 -g -fsanitize=thread)
endfunction()

# local
set(KITSBLIPS_DIR ${CMAKE_CURRENT_LIST_DIR})
FetchContent_Declare(
    KitDSP
    SOURCE_DIR ${KITSBLIPS_DIR}/kitdsp
    EXCLUDE_FROM_ALL
)
FetchContent_Declare(
    KitDSP-GPL
    SOURCE_DIR ${KITSBLIPS_DIR}/kitdsp-gpl
    EXCLUDE_FROM_ALL
)
FetchContent_Declare(
    KitGui
    SOURCE_DIR ${KITSBLIPS_DIR}/kitgui
    EXCLUDE_FROM_ALL
)
FetchContent_Declare(
    clapeze
    SOURCE_DIR ${KITSBLIPS_DIR}/clapeze
    EXCLUDE_FROM_ALL
)

# TODO: switch to FetchContent?
set(IMGUI_DIR ${CMAKE_CURRENT_LIST_DIR}/sdk/imgui)
set(PUGL_DIR ${CMAKE_CURRENT_LIST_DIR}/sdk/pugl)

# core
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt
    GIT_TAG        11.2.0
    EXCLUDE_FROM_ALL
)
FetchContent_Declare(
    etl
    GIT_REPOSITORY https://github.com/ETLCPP/etl
    GIT_TAG        20.44.2
    EXCLUDE_FROM_ALL
)
FetchContent_Declare(
    clap
    GIT_REPOSITORY https://github.com/free-audio/clap.git
    GIT_TAG        1.2.7
    EXCLUDE_FROM_ALL
)
FetchContent_Declare(
    clap-wrapper
    GIT_REPOSITORY https://github.com/alloyed/clap-wrapper.git
    GIT_TAG        19f18f2c1b4f0aee13112d75695d48d6b2487d68
    EXCLUDE_FROM_ALL
)
FetchContent_Declare(
    miniz
    GIT_REPOSITORY https://github.com/richgel999/miniz.git
    GIT_TAG        3.1.0
    EXCLUDE_FROM_ALL
)
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
    EXCLUDE_FROM_ALL
)
FetchContent_Declare(
    daisy
    GIT_REPOSITORY https://github.com/alloyed/libDaisy.git
    GIT_TAG kitdsp
    EXCLUDE_FROM_ALL
)

# gfx
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(SDL_SHARED OFF)
    set(SDL_STATIC ON)
else()
    set(SDL_SHARED ON)
    set(SDL_STATIC OFF)
endif()
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.4.0
    EXCLUDE_FROM_ALL
    FIND_PACKAGE_ARGS
)

set(CORRADE_BUILD_STATIC ON)
set(MAGNUM_BUILD_STATIC ON)
set(MAGNUM_BUILD_PLUGINS_STATIC ON)
set(MAGNUM_WITH_IMGUIINTEGRATION ON)

# opting into images, scenes, fonts
set(MAGNUM_WITH_ANYIMAGEIMPORTER ON)
set(MAGNUM_WITH_ANYSCENEIMPORTER ON)
set(MAGNUM_WITH_MAGNUMFONT ON)

# specific dependencies for above
set(MAGNUM_WITH_STBIMAGEIMPORTER ON)
set(MAGNUM_WITH_STBTRUETYPEFONT ON)
set(MAGNUM_WITH_GLTFIMPORTER ON)

# opengl
set(MAGNUM_WITH_GL ON)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(MAGNUM_WITH_WGLCONTEXT ON)
    set(MAGNUM_PLATFORMGL Magnum::WglContext)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(MAGNUM_WITH_CGLCONTEXT ON)
    set(MAGNUM_PLATFORMGL Magnum::CglContext)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(MAGNUM_WITH_EGLCONTEXT ON)
    set(MAGNUM_PLATFORMGL Magnum::EglContext)
endif()

FetchContent_Declare(
    corrade
    GIT_REPOSITORY https://github.com/mosra/corrade.git
    GIT_TAG        b65bcc0d08c8ef4b47a19ac37cdffb267e7c9994
    EXCLUDE_FROM_ALL
)
FetchContent_Declare(
    magnum
    GIT_REPOSITORY https://github.com/mosra/magnum.git
    GIT_TAG        bd52eb9be27f647cbec5475a920b79ce61953c8b
    EXCLUDE_FROM_ALL
)
FetchContent_Declare(
    magnum-plugins
    GIT_REPOSITORY https://github.com/mosra/magnum-plugins.git
    GIT_TAG        420a0e27b8a3a006a1b4db9c200139473a047b84
    EXCLUDE_FROM_ALL
)
FetchContent_Declare(
    magnum-integration
    GIT_REPOSITORY https://github.com/mosra/magnum-integration.git
    GIT_TAG 0184c8371d499d4a4f20b5982722f7942ff6f364
    EXCLUDE_FROM_ALL
)
FetchContent_Declare(
    basis_universal
    GIT_REPOSITORY https://github.com/BinomialLLC/basis_universal.git
    GIT_TAG v2_0_2
    EXCLUDE_FROM_ALL
)

# debug/test
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.17.0
    EXCLUDE_FROM_ALL
)
FetchContent_Declare(
    cpptrace
    GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
    GIT_TAG        v1.0.0
    EXCLUDE_FROM_ALL
)
FetchContent_Declare(
    AudioFile
    GIT_REPOSITORY https://github.com/adamstark/AudioFile.git
    GIT_TAG        1.1.4
    EXCLUDE_FROM_ALL
)
FetchContent_Declare(
    sentry-native
    GIT_REPOSITORY https://github.com/getsentry/sentry-native.git
    GIT_TAG        0.12.3
    EXCLUDE_FROM_ALL
)
FetchContent_Declare(
    tracy
    GIT_REPOSITORY https://github.com/wolfpld/tracy.git
    GIT_TAG v0.13.1
    EXCLUDE_FROM_ALL
)
