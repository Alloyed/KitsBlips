cmake_minimum_required(VERSION 3.22)
cmake_policy(SET CMP0048 NEW)

set(CMAKE_CXX_STANDARD 20)

# Inside your own application, you can include kitgui like so:
# 
#    FetchContent_Declare(
#        kitgui
#        SOURCE_DIR ${KITSBLIPS_DIR}/kitgui
#        EXCLUDE_FROM_ALL
#    )
#    FetchContent_MakeAvailable(kitgui)
#
# and it'll be available to link, as below.

add_executable(demo demo.cpp)
target_link_libraries(demo kitgui)

add_executable(multiwindow multiwindow.cpp)
target_link_libraries(multiwindow kitgui)

add_executable(3dscene 3dscene.cpp)
target_link_libraries(3dscene kitgui)

add_executable(immediatemode immediatemode.cpp)
target_link_libraries(immediatemode kitgui)