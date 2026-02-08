FetchContent_MakeAvailable(sentry-native)

if(NOT DEFINED ENV{KITSBLIPS_SENTRY_DSN})
    message(FATAL_ERROR "Sentry requires the environment variable KITSBLIPS_SENTRY_DSN to be defined")
endif()

if(NOT DEFINED ENV{SENTRY_AUTH_TOKEN})
    message(FATAL_ERROR "Sentry requires the environment variable SENTRY_AUTH_TOKEN to be defined")
endif()

if(WIN32)
    set(SENTRY_CLI "${CMAKE_CURRENT_SOURCE_DIR}/../sdk/sentry-cli/sentry-cli-Windows-x86_64.exe")
elseif(APPLE)
    set(SENTRY_CLI "${CMAKE_CURRENT_SOURCE_DIR}/../sdk/sentry-cli/sentry-cli-Darwin-universal")
else()
    set(SENTRY_CLI "${CMAKE_CURRENT_SOURCE_DIR}/../sdk/sentry-cli/sentry-cli-Linux-x86_64")
endif()

# arguments: every target you'd like to upload to sentry
# exposes three relevant targets. 
# * `sentry` native sdk, link to this
# * `sentry-crashpad-handler` needed in final asset directory
# * `sentry-upload` uploads debug symbols
function(enable_sentry)
    message(STATUS "Building with sentry.io telemetry enabled...")

    # dummy target used to re-add the handler to the default build without pulling in the rest of sentry
    add_custom_target(sentry-crashpad-handler ALL
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:crashpad_handler>"  "${CMAKE_CURRENT_BINARY_DIR}/dist"
    )
    add_dependencies(sentry-crashpad-handler crashpad_handler)

    # TODO: build up this list iteratively instead?
    if(WIN32)
        list(APPEND DEBUG_FILES ${ARGV})
        list(TRANSFORM DEBUG_FILES PREPEND "$<TARGET_PDB_FILE:")
        list(TRANSFORM DEBUG_FILES APPEND ">")
    else()
        list(APPEND DEBUG_FILES ${ARGV})
        list(TRANSFORM DEBUG_FILES PREPEND "$<TARGET_FILE:")
        list(TRANSFORM DEBUG_FILES APPEND ">")
    endif()
    #message(ARGV="${ARGV}")
    #message(DEBUG_FILES="${DEBUG_FILES}")

    set(DRY_RUN "--no-upload")
    if(KITSBLIPS_RETAIL)
        set(DRY_RUN "")
    endif()
    add_custom_target(sentry-upload ALL
        DEPENDS ${ARGV}
        COMMAND ${SENTRY_CLI} upload-dif ${DRY_RUN} --org kitsblips --project kitsblips ${DEBUG_FILES}
        COMMENT "Uploading debug symbols"
    )
endfunction()