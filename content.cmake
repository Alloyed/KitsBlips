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
    clap-helpers
    GIT_REPOSITORY https://github.com/free-audio/clap-helpers.git
    GIT_TAG main
)
FetchContent_Declare(
    clap-wrapper
    GIT_REPOSITORY https://github.com/free-audio/clap-wrapper.git
    GIT_TAG main
)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(SDL_SHARED OFF)
set(SDL_STATIC ON)
set(SDL_TESTS OFF)
# SDL subsystems
set(SDL_AUDIO    OFF)
set(SDL_VIDEO    ON)
set(SDL_GPU      OFF)
set(SDL_RENDER   OFF)
set(SDL_CAMERA   OFF)
set(SDL_JOYSTICK OFF)
set(SDL_HAPTIC   OFF)
set(SDL_HIDAPI   OFF)
set(SDL_POWER    OFF)
set(SDL_SENSOR   OFF)
set(SDL_DIALOG   OFF)
# https://github.com/libsdl-org/SDL/issues/6454
if(APPLE)
    enable_language(OBJC)
endif()
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.14
)
