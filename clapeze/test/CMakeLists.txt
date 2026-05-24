set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(BUILD_GTEST ON CACHE BOOL "" FORCE)

include(../../content.cmake)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

add_executable(factory-test factory.test.cpp)
target_link_libraries(factory-test clapeze gtest_main)
gtest_discover_tests(factory-test)

add_executable(effect-test effect.test.cpp)
target_link_libraries(effect-test clapeze gtest_main)
gtest_discover_tests(effect-test)

add_executable(instrument-test instrument.test.cpp)
target_link_libraries(instrument-test clapeze gtest_main)
gtest_discover_tests(instrument-test)

add_executable(transport-test transport.test.cpp)
target_link_libraries(transport-test clapeze gtest_main)
gtest_discover_tests(transport-test)

add_executable(params-test params.test.cpp)
target_link_libraries(params-test clapeze gtest_main)
gtest_discover_tests(params-test)