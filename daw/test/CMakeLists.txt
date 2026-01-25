set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(BUILD_GTEST ON CACHE BOOL "" FORCE)

include(../../content.cmake)
FetchContent_MakeAvailable(googletest AudioFile)

add_executable(testdaw
    basic.test.cpp
)
target_link_libraries(testdaw
    clap
    KitsBlips
    AudioFile
    gtest_main
)
add_test(NAME testdaw COMMAND testdaw)

if(KITSBLIPS_ENABLE_GUI)
    FetchContent_MakeAvailable(kitgui)
    add_executable(testgui
        gui.test.cpp
    )
    target_link_libraries(testgui
        clap
        KitsBlips
    )
endif()

add_library(exampleplugin MODULE
    ../src/gui/kitguiFeature.cpp
    ../src/example/example.cpp
    ../src/tester/tester.cpp
    ../src/entry.cpp
)
target_compile_definitions(exampleplugin PUBLIC
    PRODUCT_NAME="EXAMPLE"
    PRODUCT_ID="kitsblips.example"
    PRODUCT_VERSION="0.0.0"
)
target_include_directories(exampleplugin PUBLIC ../src)
target_link_libraries(exampleplugin PUBLIC
    clapeze
    KitDSP
)
if(KITSBLIPS_ENABLE_GUI)
    FetchContent_MakeAvailable(kitgui)

    target_compile_definitions(exampleplugin PUBLIC KITSBLIPS_ENABLE_GUI=1)
    target_link_libraries(exampleplugin PRIVATE kitgui)
endif()