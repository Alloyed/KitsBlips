set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(BUILD_GTEST ON CACHE BOOL "" FORCE)

include(../../content.cmake)
FetchContent_MakeAvailable(googletest AudioFile)

add_executable(testdsp
    trapfpe.cpp
    osc/dsfOscillator.test.cpp
    osc/oscillatorSweeps.test.cpp
    apps/snesEcho.test.cpp
    apps/snesBitcrush.test.cpp
    apps/psxReverb.test.cpp
    apps/chorus.test.cpp
    apps/harmonizer.test.cpp
    apps/frequencyShifter.test.cpp
    control/adsr.test.cpp
    pitch.test.cpp
    math/stft.test.cpp
)

target_include_directories(testdsp PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
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
