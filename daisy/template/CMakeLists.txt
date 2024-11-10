project(Template VERSION 0.0.1)

set(FIRMWARE_NAME Template)
set(FIRMWARE_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/src/template.cpp
)
#set(FIRMWARE_DEBUG_OPT_LEVEL -O0) # (optional) to customize Debug optimization level
#set(FIRMWARE_RELEASE_OPT_LEVEL -O2) # (optional) to customize Release optimization level

# DaisyProject uses FIRMWARE_NAME and FIRMWARE_SOURCES to build a target called ${FIRMWARE_NAME}
include(DaisyProject)

target_include_directories(${FIRMWARE_NAME} PUBLIC include ${CMAKE_CURRENT_LIST_DIR}/../shared/include)
target_link_libraries(${FIRMWARE_NAME} PRIVATE KitDSP DaisySP)
