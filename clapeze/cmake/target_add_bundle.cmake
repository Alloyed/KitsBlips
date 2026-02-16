FetchContent_MakeAvailable(miniz)
add_executable(create-zip EXCLUDE_FROM_ALL "${CMAKE_CURRENT_LIST_DIR}/../tools/createZip.cpp")
target_link_libraries(create-zip PRIVATE miniz)

# Use to add a bundled output to your library/executable target
function(target_add_bundle TARGET_NAME OUT_FILE)
    if (NOT TARGET ${TARGET_NAME})
        message(FATAL_ERROR "Target '${TARGET_NAME}' does not exist")
    endif()
    set(SOURCES ${ARGN})

    # make zip
    set(ZIP_DIR "${CMAKE_CURRENT_BINARY_DIR}/tmp")
    set(ZIP_FILE "${ZIP_DIR}/${TARGET_NAME}_assets.zip")

    # this command would work, except for the fact that we want STORE compression (aka, none).
    #COMMAND ${CMAKE_COMMAND} -E tar c "${ZIP_FILE}" --format=zip "${ASSETS_DIR}"
    #for convenience just wrote our own zip tool with pieces laying around
    add_custom_command(
        OUTPUT ${ZIP_FILE}
        DEPENDS create-zip ${SOURCES}
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" # use so final zip looks like project dir
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ZIP_DIR}"
        COMMAND ${CMAKE_COMMAND} -E rm -f "${ZIP_FILE}"
        COMMAND create-zip "${ZIP_FILE}" ${SOURCES}
        COMMENT "creating zip file: ${ZIP_FILE}"
        VERBATIM
    )

    # make bundle
    set(TARGET_FILE "$<TARGET_FILE:${TARGET_NAME}>")
    cmake_path(GET OUT_FILE PARENT_PATH OUT_DIR)
    add_custom_command(
        OUTPUT ${OUT_FILE}
        DEPENDS ${TARGET_NAME} ${ZIP_FILE}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E cat "${TARGET_FILE}" "${ZIP_FILE}" > "${OUT_FILE}"
        COMMENT "adding asset bundle to: ${OUT_FILE}"
        VERBATIM
    )
    add_custom_target(${TARGET_NAME}_bundled ALL SOURCES ${OUT_FILE})
endfunction()
