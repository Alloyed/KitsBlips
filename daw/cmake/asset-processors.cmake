function(wip_asset_processors)
    # this isn't hooked up yet, just collecting cli commands

    # gltfpack example
    # see https://github.com/zeux/meshoptimizer
    add_custom_command(
        OUTPUT ${OUT}
        DEPENDS ${IN}
        # -cc: extra compression
        # -tc: teture compression (basisisu)
        # -mi: do mesh instancing
        COMMAND gltfpack -cc -tc -mi -i "${IN}" -o "${OUT}"
        VERBATIM
    )

    # sentry-cli debug-files upload -o <org> -p <project> ${DIST_FOLDER}
endfunction()
