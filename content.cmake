# for clap-wrapper
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15")
    enable_language(OBJC OBJCXX)
endif()

# with this flag we can use set() to set options for the following packages
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

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
FetchContent_Declare(
    KitGui
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/kitgui
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
FetchContent_Declare(
    cpptrace
    GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
    GIT_TAG        v1.0.0
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

set(PHYSFS_ARCHIVE_ZIP ON)
set(PHYSFS_ARCHIVE_7Z OFF)
set(PHYSFS_ARCHIVE_GRP OFF)
set(PHYSFS_ARCHIVE_WAD OFF)
set(PHYSFS_ARCHIVE_CSM OFF)
set(PHYSFS_ARCHIVE_HOG OFF)
set(PHYSFS_ARCHIVE_MVL OFF)
set(PHYSFS_ARCHIVE_QPAK OFF)
set(PHYSFS_ARCHIVE_SLB OFF)
set(PHYSFS_ARCHIVE_ISO9660 OFF)
set(PHYSFS_ARCHIVE_VDF OFF)
set(PHYSFS_ARCHIVE_LECARCHIVES OFF)
set(PHYSFS_BUILD_STATIC ON)
set(PHYSFS_BUILD_SHARED OFF)
set(PHYSFS_BUILD_TEST OFF)
set(PHYSFS_DISABLE_INSTALL ON)
set(PHYSFS_BUILD_DOCS OFF)
FetchContent_Declare(
	physfs
	GIT_REPOSITORY	https://github.com/icculus/physfs.git
	GIT_TAG 	adfdec6af14e4d12551446e7ad060415ca563950
)

set(MAGNUM_BUILD_STATIC ON)
set(MAGNUM_BUILD_PLUGINS_STATIC ON)

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
    GIT_TAG        8b3c02277020d9c609f54200d050ff665c4431e1
)
FetchContent_Declare(
    magnum
    GIT_REPOSITORY https://github.com/mosra/magnum.git
    GIT_TAG        be38d5e2bbd03cab4f31707d8a012a4ce119fc40
)
FetchContent_Declare(
    magnum-plugins
    GIT_REPOSITORY https://github.com/mosra/magnum-plugins.git
    GIT_TAG        9f31cf5a0a6a44a63e320924141513d473586760
)