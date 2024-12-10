set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(BUILD_GTEST ON CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.15.2
)

FetchContent_MakeAvailable(googletest)

add_executable(testdsp_gpl
  pitch.test.cpp
)

target_link_libraries(testdsp_gpl
    KitDSP
    KitDSP_GPL
    gtest_main
)

add_test(NAME testdsp_gpl
         COMMAND testdsp_gpl)