set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(BUILD_GTEST ON CACHE BOOL "" FORCE)

include(../../content.cmake)
FetchContent_MakeAvailable(googletest)

add_executable(factory-test factory.test.cpp)
target_link_libraries(factory-test clapeze gtest_main)
add_test(NAME factory-test COMMAND factory-test)

add_executable(effect-test effect.test.cpp)
target_link_libraries(effect-test
    clapeze
    gtest_main
)
add_test(NAME effect-test COMMAND effect-test)

add_executable(instrument-test instrument.test.cpp)
target_link_libraries(instrument-test
    clapeze
    gtest_main
)
add_test(NAME instrument-test COMMAND instrument-test)

add_executable(transport-test transport.test.cpp)
target_link_libraries(transport-test
    clapeze
    gtest_main
)
add_test(NAME transport-test COMMAND transport-test)