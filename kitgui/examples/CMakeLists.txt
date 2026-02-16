cmake_minimum_required(VERSION 3.22)
cmake_policy(SET CMP0048 NEW)

set(CMAKE_CXX_STANDARD 20)
add_compile_definitions(-DPROJECT_DIR="${CMAKE_SOURCE_DIR}")

# include(../cmake/asset-cook.cmake)
# add_packed_scene(${CMAKE_SOURCE_DIR}/assets/suzanne.glb ${CMAKE_SOURCE_DIR}/assets/suzanne.basis.glb)

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

add_executable(imguiDemo imguiDemo.cpp)
target_link_libraries(imguiDemo kitgui)

add_executable(multiwindow multiwindow.cpp)
target_link_libraries(multiwindow kitgui)

add_executable(3dviewer 3dviewer.cpp)
target_link_libraries(3dviewer kitgui)

add_executable(customControls customControls.cpp)
target_link_libraries(customControls kitgui)
