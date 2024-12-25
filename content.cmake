include(FetchContent)

# local
FetchContent_Declare(
    daisy
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/sdk/libDaisy
)
FetchContent_Declare(
    CMSIS_5 
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/sdk/libDaisy/Drivers/CMSIS_5
)
FetchContent_Declare(
    CMSIS-DSP
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/sdk/libDaisy/Drivers/CMSIS-DSP
)
FetchContent_Declare(
    KitDSP
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/kitdsp
)
FetchContent_Declare(
    KitDSP-GPL
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/kitdsp-gpl
)

# remote
FetchContent_Declare(
    etl
    GIT_REPOSITORY https://github.com/ETLCPP/etl
    GIT_TAG        20.39.4
)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
)
