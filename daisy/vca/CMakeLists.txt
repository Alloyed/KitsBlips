project(Vca VERSION 0.0.1)

set(FIRMWARE_NAME Vca)
set(FIRMWARE_SOURCES
  vca.cpp
)

# DaisyProject uses FIRMWARE_NAME and FIRMWARE_SOURCES to build a target called ${FIRMWARE_NAME}
include(DaisyProject)

target_include_directories(${FIRMWARE_NAME} PUBLIC include ../shared/include)
target_link_libraries(${FIRMWARE_NAME} PRIVATE KitDSP)
# uncomment to include in CI artifact
#install(TARGETS ${FIRMWARE_NAME} DESTINATION ${DAISY_STORAGE})