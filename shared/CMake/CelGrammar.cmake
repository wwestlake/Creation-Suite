# Wraps WinFlexBison as CMake custom commands for the shared CEL
# frontend. This is copied from the working Creation Engine setup so the
# Suite layer uses the real grammar-generation path rather than
# hand-maintained generated files.

if(DEFINED CE_WINFLEXBISON_DIR)
    find_program(CE_BISON_EXE NAMES win_bison bison PATHS "${CE_WINFLEXBISON_DIR}" NO_DEFAULT_PATH)
    find_program(CE_FLEX_EXE NAMES win_flex flex PATHS "${CE_WINFLEXBISON_DIR}" NO_DEFAULT_PATH)
else()
    find_program(CE_BISON_EXE NAMES win_bison bison PATHS "D:/tools/winflexbison" "C:/tools/winflexbison")
    find_program(CE_FLEX_EXE NAMES win_flex flex PATHS "D:/tools/winflexbison" "C:/tools/winflexbison")
endif()

if(NOT CE_BISON_EXE OR NOT CE_FLEX_EXE)
    message(FATAL_ERROR
        "CEL grammar tooling not found (bison=${CE_BISON_EXE} flex=${CE_FLEX_EXE}). "
        "Set -DCE_WINFLEXBISON_DIR=<directory containing win_bison.exe/win_flex.exe> "
        "(get WinFlexBison from https://github.com/lexxmark/winflexbison/releases), "
        "or install bison/flex on PATH.")
endif()

message(STATUS "Suite CEL grammar: bison=${CE_BISON_EXE} flex=${CE_FLEX_EXE}")

get_filename_component(CE_FLEX_TOOL_DIR "${CE_FLEX_EXE}" DIRECTORY)
find_path(CE_FLEXLEXER_INCLUDE_DIR NAMES FlexLexer.h PATHS "${CE_FLEX_TOOL_DIR}" NO_DEFAULT_PATH)
if(NOT CE_FLEXLEXER_INCLUDE_DIR)
    message(FATAL_ERROR "FlexLexer.h not found next to ${CE_FLEX_EXE} (expected alongside win_flex.exe in the WinFlexBison distribution).")
endif()

function(ce_add_cel_grammar out_var)
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
endfunction()
