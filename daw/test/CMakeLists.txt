set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(BUILD_GTEST ON CACHE BOOL "" FORCE)

include(../../content.cmake)
FetchContent_MakeAvailable(googletest AudioFile)

if(KITSBLIPS_ENABLE_GUI)
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

    add_executable(testgui
        gui.test.cpp
    )
    target_link_libraries(testgui
        clap
        KitsBlips
    )
    # not a test exe
 endif()