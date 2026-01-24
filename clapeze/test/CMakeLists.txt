set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(BUILD_GTEST ON CACHE BOOL "" FORCE)

include(../../content.cmake)
FetchContent_MakeAvailable(googletest)

add_executable(test-clapeze
  factory.test.cpp
  ../examples/effectExample.cpp
  ../examples/instrumentExample.cpp
)

target_link_libraries(test-clapeze
    clapeze
    gtest_main
)

add_test(NAME test-clapeze
         COMMAND test-clapeze)