get_filename_component(CREATION_SUITE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

function(creation_suite_register_bin_link target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "creation_suite_register_bin_link: target '${target_name}' does not exist")
    endif()

    add_custom_command(TARGET "${target_name}" POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory
                "${CREATION_SUITE_ROOT}/bin/$<CONFIG>"
        COMMAND "${CMAKE_COMMAND}" -E rm -f
                "${CREATION_SUITE_ROOT}/bin/$<CONFIG>/$<TARGET_FILE_NAME:${target_name}>"
        COMMAND "${CMAKE_COMMAND}" -E create_symlink
                "$<TARGET_FILE:${target_name}>"
                "${CREATION_SUITE_ROOT}/bin/$<CONFIG>/$<TARGET_FILE_NAME:${target_name}>"
        VERBATIM
    )
endfunction()
