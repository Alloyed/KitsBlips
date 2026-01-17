# Use to add a bundled output to your library/executable target
function(target_add_bundle TARGET_NAME ASSETS_DIR OUT_FILE)
    if (NOT TARGET ${TARGET_NAME})
        message(FATAL_ERROR "Target '${TARGET_NAME}' does not exist")
    endif()
    if (NOT IS_DIRECTORY ${ASSETS_DIR})
        message(FATAL_ERROR "Directory '${ASSETS_DIR}' does not exist")
    endif()

    # make zip
    set(ZIP_DIR "${CMAKE_CURRENT_BINARY_DIR}/tmp")
    set(ZIP_FILE "${ZIP_DIR}/${TARGET_NAME}_assets.zip")
    cmake_path(GET ASSETS_DIR PARENT_PATH WORKING_DIR)
    file(GLOB_RECURSE GLOBBED_ASSETS CONFIGURE_DEPENDS "${ASSETS_DIR}/*")
    add_custom_command(
        OUTPUT ${ZIP_FILE}
        DEPENDS ${GLOBBED_ASSETS}
        WORKING_DIRECTORY "${WORKING_DIR}" # use so final zip looks like project dir
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ZIP_DIR}"
        COMMAND ${CMAKE_COMMAND} -E tar c "${ZIP_FILE}" --format=zip "${ASSETS_DIR}"
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
