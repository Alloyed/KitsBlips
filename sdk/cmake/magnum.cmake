# doc.magnum.graphics

# for debugging
set(KITSBLIPS_USE_BASIS OFF)

set(CORRADE_BUILD_STATIC ON)
set(MAGNUM_BUILD_STATIC ON)
set(MAGNUM_BUILD_PLUGINS_STATIC ON)
set(MAGNUM_WITH_IMGUIINTEGRATION ON)

# opting into images, scenes
set(MAGNUM_WITH_ANYSCENEIMPORTER ON)
set(MAGNUM_WITH_ANYIMAGEIMPORTER ON)

# specific dependencies for above
set(MAGNUM_WITH_GLTFIMPORTER ON)
set(MAGNUM_WITH_STBIMAGEIMPORTER ON)

# basis is a little chunky but provides very fast texture loads
if(KITSBLIPS_USE_BASIS)
    add_definitions(
        #-DBASISD_SUPPORT_KTX2=0
        #-DBASISD_SUPPORT_KTX2_ZSTD=0
        #-DBASISD_SUPPORT_BC7=0
        #-DBASISD_SUPPORT_ATC=0
    )
    set(MAGNUM_WITH_KTXIMPORTER ON)
    set(MAGNUM_WITH_BASISIMPORTER ON)
endif()

# Tools
# opting into images, scenes
if(KITSBLIPS_USE_BASIS)
    set(MAGNUM_WITH_ANYSCENECONVERTER ON)
    set(MAGNUM_WITH_GLTFSCENECONVERTER ON)
    set(MAGNUM_WITH_ANYIMAGECONVERTER ON)
    set(MAGNUM_WITH_STBIMAGECONVERTER ON)
    set(MAGNUM_WITH_KTXIMAGECONVERTER ON)
    set(MAGNUM_WITH_BASISIMAGECONVERTER ON)

    set(MAGNUM_WITH_SCENECONVERTER ON)
    set(MAGNUM_SCENECONVERTER_STATIC_PLUGINS "Magnum::AnySceneImporter;Magnum::AnySceneConverter;MagnumPlugins::GltfImporter;MagnumPlugins::GltfSceneConverter;Magnum::AnyImageImporter;MagnumPlugins::StbImageImporter;MagnumPlugins::StbImageConverter;MagnumPlugins::KtxImporter;MagnumPlugins::BasisImporter;MagnumPlugins::BasisImageConverter")
    set(MAGNUM_WITH_IMAGECONVERTER ON)
    set(MAGNUM_IMAGECONVERTER_STATIC_PLUGINS ${MAGNUM_SCENECONVERTER_STATIC_PLUGINS})
endif()

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
    SYSTEM
    EXCLUDE_FROM_ALL
)
FetchContent_Declare(
    magnum
    GIT_REPOSITORY https://github.com/mosra/magnum.git
    GIT_TAG        bd52eb9be27f647cbec5475a920b79ce61953c8b
    SYSTEM
    EXCLUDE_FROM_ALL
)
FetchContent_Declare(
    magnum-plugins
    GIT_REPOSITORY https://github.com/mosra/magnum-plugins.git
    GIT_TAG        420a0e27b8a3a006a1b4db9c200139473a047b84
    SYSTEM
    EXCLUDE_FROM_ALL
)
FetchContent_Declare(
    magnum-integration
    GIT_REPOSITORY https://github.com/mosra/magnum-integration.git
    GIT_TAG 0184c8371d499d4a4f20b5982722f7942ff6f364
    SYSTEM
    EXCLUDE_FROM_ALL
)
FetchContent_Declare(
    basis_universal
    GIT_REPOSITORY https://github.com/BinomialLLC/basis_universal.git
    #GIT_TAG        v2_0_2
    GIT_TAG v1_50_0_2
    SYSTEM
    EXCLUDE_FROM_ALL
    FIND_PACKAGE_ARGS
)
