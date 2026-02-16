FetchContent_MakeAvailable(magnum magnum-plugins)

add_custom_target(tmp_bundles ALL)
function(add_packed_scene INPUT OUTPUT)
    add_custom_command(
        OUTPUT ${OUTPUT}
        DEPENDS ${INPUT}
        COMMAND Magnum::sceneconverter "${INPUT}" "${OUTPUT}" -c imageConverter=BasisKtxImageConverter,imageConverter/uastc 
        VERBATIM
    )
    target_sources(tmp_bundles PRIVATE ${OUTPUT})
endfunction()

function(add_packed_image INPUT OUTPUT)
    add_custom_command(
        OUTPUT ${OUTPUT}
        DEPENDS ${INPUT}
        COMMAND Magnum::imageconverter "${INPUT}" "${OUTPUT}" -C BasisKtxImageConverter
        VERBATIM
    )
    target_sources(tmp_bundles PRIVATE ${OUTPUT})
endfunction()
