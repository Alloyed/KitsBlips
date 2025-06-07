# for clap-wrapper
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15")
    enable_language(OBJC OBJCXX)
endif()

include(FetchContent)

# local
FetchContent_Declare(
    daisy
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/sdk/libDaisy
)
FetchContent_Declare(
    CMSIS_5 
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/sdk/libDaisy/Drivers/CMSIS_5
)
FetchContent_Declare(
    CMSIS-DSP
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/sdk/libDaisy/Drivers/CMSIS-DSP
)
FetchContent_Declare(
    KitDSP
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/kitdsp
)
FetchContent_Declare(
    KitDSP-GPL
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/kitdsp-gpl
)

# TODO: switch to FetchContent?
set(IMGUI_DIR ${CMAKE_CURRENT_LIST_DIR}/sdk/imgui)

# remote
FetchContent_Declare(
    etl
    GIT_REPOSITORY https://github.com/ETLCPP/etl
    GIT_TAG        20.39.4
)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
)
FetchContent_Declare(
    clap
    GIT_REPOSITORY https://github.com/free-audio/clap.git
    GIT_TAG        1.2.6
)
FetchContent_Declare(
    clap-wrapper
    GIT_REPOSITORY https://github.com/free-audio/clap-wrapper.git
    GIT_TAG main
)

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
    GIT_TAG release-3.2.14
    FIND_PACKAGE_ARGS
)
