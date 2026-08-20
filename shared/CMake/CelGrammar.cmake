include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/CreationSuiteBuildSettings.cmake")
creation_suite_init_build_settings()

# Wraps WinFlexBison as CMake custom commands for the shared CEL
# frontend. This is copied from the working Creation Engine setup so the
# Suite layer uses the real grammar-generation path rather than
# hand-maintained generated files.

set(CE_CEL_GRAMMAR_FALLBACK_DIR "${CMAKE_CURRENT_LIST_DIR}/../CEL/generated")

if(DEFINED CE_WINFLEXBISON_DIR)
    find_program(CE_BISON_EXE NAMES win_bison bison PATHS "${CE_WINFLEXBISON_DIR}" NO_DEFAULT_PATH)
    find_program(CE_FLEX_EXE NAMES win_flex flex PATHS "${CE_WINFLEXBISON_DIR}" NO_DEFAULT_PATH)
else()
    find_program(CE_BISON_EXE NAMES win_bison bison)
    find_program(CE_FLEX_EXE NAMES win_flex flex)
endif()

set(CE_CEL_CAN_REGENERATE_GRAMMAR TRUE)
if(NOT CE_BISON_EXE OR NOT CE_FLEX_EXE)
    set(CE_CEL_CAN_REGENERATE_GRAMMAR FALSE)
endif()

if(CE_CEL_CAN_REGENERATE_GRAMMAR)
    message(STATUS "Suite CEL grammar: bison=${CE_BISON_EXE} flex=${CE_FLEX_EXE}")

    get_filename_component(CE_FLEX_TOOL_DIR "${CE_FLEX_EXE}" DIRECTORY)
    find_path(CE_FLEXLEXER_INCLUDE_DIR NAMES FlexLexer.h PATHS "${CE_FLEX_TOOL_DIR}" NO_DEFAULT_PATH)
    if(NOT CE_FLEXLEXER_INCLUDE_DIR)
        message(FATAL_ERROR "FlexLexer.h not found next to ${CE_FLEX_EXE} (expected alongside win_flex.exe in the WinFlexBison distribution).")
    endif()
else()
    set(CE_FLEXLEXER_INCLUDE_DIR "${CE_CEL_GRAMMAR_FALLBACK_DIR}")

    foreach(_fallback_file parser.cpp parser.hpp location.hh lexer.cpp FlexLexer.h)
        if(NOT EXISTS "${CE_CEL_GRAMMAR_FALLBACK_DIR}/${_fallback_file}")
            message(FATAL_ERROR
                "CEL grammar tooling not found (bison=${CE_BISON_EXE} flex=${CE_FLEX_EXE}) and fallback file "
                "'${CE_CEL_GRAMMAR_FALLBACK_DIR}/${_fallback_file}' is missing. "
                "Install WinFlexBison and set -DCE_WINFLEXBISON_DIR=<directory containing win_bison.exe/win_flex.exe>, "
                "or restore the generated fallback files under shared/CEL/generated.")
        endif()
    endforeach()

    message(WARNING
        "Suite CEL grammar tooling not found (bison=${CE_BISON_EXE} flex=${CE_FLEX_EXE}); "
        "using checked-in generated sources from ${CE_CEL_GRAMMAR_FALLBACK_DIR}. "
        "Install WinFlexBison and set CE_WINFLEXBISON_DIR to regenerate from grammar/cel.y and grammar/cel.l.")
endif()

function(ce_add_cel_grammar out_var)
    if(CE_CEL_CAN_REGENERATE_GRAMMAR)
        set(gen_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
        file(MAKE_DIRECTORY "${gen_dir}")

        set(parser_cpp "${gen_dir}/parser.cpp")
        set(parser_hpp "${gen_dir}/parser.hpp")
        set(location_hh "${gen_dir}/location.hh")
        set(lexer_cpp "${gen_dir}/lexer.cpp")

        add_custom_command(
            OUTPUT "${parser_cpp}" "${parser_hpp}" "${location_hh}"
            COMMAND "${CE_BISON_EXE}"
                    "-Werror=conflicts-sr" "-Werror=conflicts-rr"
                    "--defines=${parser_hpp}"
                    "--output=${parser_cpp}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/grammar/cel.y"
            WORKING_DIRECTORY "${gen_dir}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/grammar/cel.y"
            COMMENT "Suite CEL: generating parser from cel.y (bison)"
            VERBATIM
        )

        add_custom_command(
            OUTPUT "${lexer_cpp}"
            COMMAND "${CE_FLEX_EXE}"
                    "--outfile=${lexer_cpp}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/grammar/cel.l"
            WORKING_DIRECTORY "${gen_dir}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/grammar/cel.l" "${parser_hpp}"
            COMMENT "Suite CEL: generating lexer from cel.l (flex)"
            VERBATIM
        )

        set(${out_var} "${parser_cpp}" "${lexer_cpp}" PARENT_SCOPE)
        set(CE_CEL_GRAMMAR_GEN_DIR "${gen_dir}" PARENT_SCOPE)
    else()
        set(${out_var}
            "${CE_CEL_GRAMMAR_FALLBACK_DIR}/parser.cpp"
            "${CE_CEL_GRAMMAR_FALLBACK_DIR}/lexer.cpp"
            PARENT_SCOPE)
        set(CE_CEL_GRAMMAR_GEN_DIR "${CE_CEL_GRAMMAR_FALLBACK_DIR}" PARENT_SCOPE)
    endif()
endfunction()
