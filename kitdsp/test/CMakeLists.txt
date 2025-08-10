set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(BUILD_GTEST ON CACHE BOOL "" FORCE)

include(../../content.cmake)
FetchContent_MakeAvailable(googletest AudioFile)

add_executable(testdsp
    dsfOscillator.test.cpp
    oscillatorSweeps.test.cpp
    snesEcho.test.cpp
    psxReverb.test.cpp
    chorus.test.cpp
    harmonizer.test.cpp
    frequencyShifter.test.cpp
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
