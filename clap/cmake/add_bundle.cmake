function(add_bundle TARGET_NAME OUTPUT_FILE ASSETS_DIR)
    if (NOT TARGET ${TARGET_NAME})
        message(FATAL_ERROR "Target '${TARGET_NAME}' does not exist")
    endif()
    if (NOT IS_DIRECTORY ${ASSETS_DIR})
        message(FATAL_ERROR "Directory '${ASSETS_DIR}' does not exist")
    endif()

    set(ZIP_DIR "${CMAKE_CURRENT_BINARY_DIR}/tmp")
    set(ZIP_FILE "${ZIP_DIR}/${TARGET_NAME}_assets.zip")
    set(TARGET_FILE "$<TARGET_FILE:${TARGET_NAME}>")
    cmake_path(GET ASSETS_DIR PARENT_PATH WORKING_DIR)
    cmake_path(GET OUTPUT_FILE PARENT_PATH OUTPUT_DIR)

    add_custom_command(
        TARGET ${TARGET_NAME} POST_BUILD
        WORKING_DIRECTORY "${WORKING_DIR}" # use so final zip looks like project dir
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ZIP_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E tar c "${ZIP_FILE}" --format=zip "${ASSETS_DIR}"
        COMMAND ${CMAKE_COMMAND} -E cat "${TARGET_FILE}" "${ZIP_FILE}" > "${OUTPUT_FILE}"
        #COMMAND ${CMAKE_COMMAND} -E rm "${ZIP_FILE}"
        COMMENT "Creating embedded asset library for ${TARGET_NAME}"
        VERBATIM
    )
endfunction()
