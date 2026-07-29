include("${CMAKE_CURRENT_LIST_DIR}/../CMake/CelGrammar.cmake")

ce_add_cel_grammar(CE_CEL_GRAMMAR_SOURCES)

add_library(creation_suite_cel_frontend STATIC
    include/lang/source_location.h
    include/lang/diagnostics.h
    include/lang/type.h
    include/lang/ast.h
    include/lang/ast_printer.h
    include/lang/lexer.h
    include/lang/compiler.h
    include/lang/sema.h
    include/lang/intrinsics.def
    src/source_location.cpp
    src/diagnostics.cpp
    src/type.cpp
    src/ast.cpp
    src/ast_printer.cpp
    src/compiler.cpp
    src/sema.cpp
    ${CE_CEL_GRAMMAR_SOURCES}
)

target_include_directories(creation_suite_cel_frontend
    PUBLIC include
    PRIVATE "${CE_CEL_GRAMMAR_GEN_DIR}" "${CE_FLEXLEXER_INCLUDE_DIR}"
)

target_compile_features(creation_suite_cel_frontend PUBLIC cxx_std_20)
set_source_files_properties(${CE_CEL_GRAMMAR_SOURCES} PROPERTIES COMPILE_OPTIONS "/W0")

add_executable(creation_suite_cel_frontend_smoke
    tests/CelFrontendSmoke.cpp
)

target_link_libraries(creation_suite_cel_frontend_smoke
    PRIVATE
        creation_suite_cel_frontend
)

add_test(
    NAME creation_suite_cel_frontend_smoke
    COMMAND creation_suite_cel_frontend_smoke
)
