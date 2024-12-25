set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(BUILD_GTEST ON CACHE BOOL "" FORCE)

include(../../content.cmake)
FetchContent_MakeAvailable(googletest)

add_executable(testdsp
    dsfOscillator.test.cpp
    oscillatorSweeps.test.cpp
    snesEcho.test.cpp
    psxReverb.test.cpp
)

target_link_libraries(testdsp
    KitDSP
    gtest_main
)

add_test(NAME testdsp
         COMMAND testdsp)