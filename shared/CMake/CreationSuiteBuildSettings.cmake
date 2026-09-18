include_guard(GLOBAL)

function(creation_suite_init_build_settings)
    get_filename_component(_repo_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
    get_filename_component(_default_workspaces_root "${_repo_root}/.." ABSOLUTE)
    get_filename_component(_repo_dir_name "${_repo_root}" NAME)

    if(NOT DEFINED CREATION_SUITE_REPO_ROOT OR CREATION_SUITE_REPO_ROOT STREQUAL "")
        set(CREATION_SUITE_REPO_ROOT "${_repo_root}" CACHE PATH "Absolute path to the Creation Suite umbrella repo root" FORCE)
    endif()

    if(NOT DEFINED CREATION_SUITE_WORKSPACES_ROOT OR CREATION_SUITE_WORKSPACES_ROOT STREQUAL "")
        if(DEFINED ENV{CREATION_SUITE_WORKSPACES_ROOT} AND NOT "$ENV{CREATION_SUITE_WORKSPACES_ROOT}" STREQUAL "")
            set(CREATION_SUITE_WORKSPACES_ROOT "$ENV{CREATION_SUITE_WORKSPACES_ROOT}" CACHE PATH
                "Workspace root containing agent repos and shared bin folders" FORCE)
        else()
            set(CREATION_SUITE_WORKSPACES_ROOT "${_default_workspaces_root}" CACHE PATH
                "Workspace root containing agent repos and shared bin folders" FORCE)
        endif()
    endif()

    if(NOT DEFINED CREATION_SUITE_AGENT_ID OR CREATION_SUITE_AGENT_ID STREQUAL "")
        if(DEFINED ENV{CREATION_SUITE_AGENT_ID} AND NOT "$ENV{CREATION_SUITE_AGENT_ID}" STREQUAL "")
            set(_agent_id "$ENV{CREATION_SUITE_AGENT_ID}")
        elseif(_repo_dir_name MATCHES "^CreationSuite-(.+)$")
            set(_agent_id "${CMAKE_MATCH_1}")
        else()
            set(_agent_id "suite")
        endif()

        string(TOLOWER "${_agent_id}" _agent_id)
        set(CREATION_SUITE_AGENT_ID "${_agent_id}" CACHE STRING
            "Agent/workspace identifier used for shared suite bin folders" FORCE)
    endif()

    if((NOT DEFINED JUCE_DIR OR JUCE_DIR STREQUAL "") AND DEFINED ENV{JUCE_DIR} AND NOT "$ENV{JUCE_DIR}" STREQUAL "")
        set(JUCE_DIR "$ENV{JUCE_DIR}" CACHE PATH "Path to the JUCE checkout used by the suite apps" FORCE)
    endif()

    if((NOT DEFINED CE_WINFLEXBISON_DIR OR CE_WINFLEXBISON_DIR STREQUAL "")
       AND DEFINED ENV{CE_WINFLEXBISON_DIR} AND NOT "$ENV{CE_WINFLEXBISON_DIR}" STREQUAL "")
        set(CE_WINFLEXBISON_DIR "$ENV{CE_WINFLEXBISON_DIR}" CACHE PATH
            "Directory containing win_bison.exe and win_flex.exe" FORCE)
    endif()

    if((NOT DEFINED CE_LLVM_VCPKG_DIR OR CE_LLVM_VCPKG_DIR STREQUAL "")
       AND DEFINED ENV{CE_LLVM_VCPKG_DIR} AND NOT "$ENV{CE_LLVM_VCPKG_DIR}" STREQUAL "")
        set(CE_LLVM_VCPKG_DIR "$ENV{CE_LLVM_VCPKG_DIR}" CACHE PATH
            "Path to the vcpkg_installed/x64-windows tree that contains the suite LLVM build" FORCE)
    endif()
endfunction()

function(creation_suite_require_juce)
    creation_suite_init_build_settings()

    if(NOT DEFINED JUCE_DIR OR JUCE_DIR STREQUAL "")
        message(FATAL_ERROR "JUCE_DIR is not set. Set JUCE_DIR in the environment or pass -DJUCE_DIR=<path-to-JUCE> when configuring.")
    endif()

    if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "JUCE_DIR is set to '${JUCE_DIR}', but ${JUCE_DIR}/CMakeLists.txt was not found.")
    endif()
endfunction()

function(creation_suite_get_shared_bin_dir out_var)
    creation_suite_init_build_settings()
    set(${out_var} "${CREATION_SUITE_WORKSPACES_ROOT}/${CREATION_SUITE_AGENT_ID}-$<LOWER_CASE:$<CONFIG>>-bin" PARENT_SCOPE)
endfunction()

function(creation_suite_add_shared_bin_copy target_name)
    creation_suite_get_shared_bin_dir(_shared_bin_dir)
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_shared_bin_dir}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${target_name}>" "${_shared_bin_dir}/"
        COMMENT "Copying ${target_name} to the shared ${CREATION_SUITE_AGENT_ID} bin directory"
    )
endfunction()
