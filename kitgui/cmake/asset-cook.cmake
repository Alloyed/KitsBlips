function(add_basisu_command INPUT OUTPUT)
    FetchContent_MakeAvailable(basis_universal)
    add_custom_command(
        OUTPUT ${OUTPUT}
        DEPENDS basisu ${INPUT}
        COMMAND $<TARGET_FILE:basisu> -file ${INPUT} -output-file ${OUTPUT}
        VERBATIM
    )
endfunction()

function(add_gltfpack_command INPUT OUTPUT)
    if(WIN32)
        set(GLTFPACK_CLI ${CMAKE_CURRENT_LIST_DIR}/../../sdk/gltfpack/gltfpack-win.exe)
    elseif(LINUX)
        set(GLTFPACK_CLI ${CMAKE_CURRENT_LIST_DIR}/../../sdk/gltfpack/gltfpack-lin)
    elseif(APPLE)
        set(GLTFPACK_CLI ${CMAKE_CURRENT_LIST_DIR}/../../sdk/gltfpack/gltfpack-mac)
    else()
        message(FATAL_ERROR "No gltfpack binary for this platform")
    endif()
    add_custom_command(
        OUTPUT ${OUTPUT}
        DEPENDS ${INPUT}
        COMMAND ${GLTFPACK_CLI} -v -cc -tc -mi -i "${INPUT}" -o "${OUTPUT}"
        VERBATIM
    )
endfunction()
