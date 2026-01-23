set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(BUILD_GTEST ON CACHE BOOL "" FORCE)

include(../../content.cmake)
FetchContent_MakeAvailable(googletest AudioFile)

add_executable(testdsp
    osc/dsfOscillator.test.cpp
    osc/oscillatorSweeps.test.cpp
    apps/snesEcho.test.cpp
    apps/psxReverb.test.cpp
    apps/chorus.test.cpp
    apps/harmonizer.test.cpp
    apps/frequencyShifter.test.cpp
    control/adsr.test.cpp
)

target_link_libraries(testdsp
    KitDSP
    AudioFile
    gtest_main
)

target_compile_definitions(testdsp PUBLIC
    -DPROJECT_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(NAME testdsp
         COMMAND testdsp)
