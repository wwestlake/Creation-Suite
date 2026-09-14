if(NOT DEFINED CREATION_SUITE_FRUST_SOURCE_DIR)
    get_filename_component(CREATION_SUITE_FRUST_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../third_party/FrustLang" ABSOLUTE)
endif()

if(NOT EXISTS "${CREATION_SUITE_FRUST_SOURCE_DIR}/projects/09_frust_plugin_host/CMakeLists.txt")
    message(FATAL_ERROR "FRust source is required. Initialize third_party/FrustLang.")
endif()

if(WIN32 AND DEFINED CE_WINFLEXBISON_DIR AND EXISTS "${CE_WINFLEXBISON_DIR}/win_flex.exe")
    set(FLEX_EXECUTABLE "${CE_WINFLEXBISON_DIR}/win_flex.exe" CACHE FILEPATH "FRust Flex executable" FORCE)
    set(BISON_EXECUTABLE "${CE_WINFLEXBISON_DIR}/win_bison.exe" CACHE FILEPATH "FRust Bison executable" FORCE)
    set(FRUST_WINFLEXBISON_DIR "${CE_WINFLEXBISON_DIR}" CACHE PATH "FRust WinFlexBison directory" FORCE)
endif()

if(NOT TARGET frate_lib)
    add_subdirectory("${CREATION_SUITE_FRUST_SOURCE_DIR}/projects/05_frate" "${CMAKE_CURRENT_BINARY_DIR}/FrustFrate")
endif()

if(NOT TARGET frust_lang)
    add_subdirectory("${CREATION_SUITE_FRUST_SOURCE_DIR}/projects/01_language_paradigms/02_functional" "${CMAKE_CURRENT_BINARY_DIR}/FrustCompiler")
endif()

if(NOT TARGET frust_plugin_host)
    add_subdirectory("${CREATION_SUITE_FRUST_SOURCE_DIR}/projects/09_frust_plugin_host" "${CMAKE_CURRENT_BINARY_DIR}/FrustPluginHost")
endif()

if(MSVC)
    # FRust's upstream targets share their archive/PDB output directory.
    # /FS keeps parallel compiler invocations from racing the same PDB.
    foreach(frust_target frate_lib frust_lang frust_plugin_host)
        if(TARGET ${frust_target})
            target_compile_options(${frust_target} PRIVATE /FS /MP1)
        endif()
    endforeach()
endif()

add_library(creation_suite_frust_plugin_runtime STATIC
    include/creation/frust/PluginRuntime.h
    src/PluginRuntime.cpp
)

target_include_directories(creation_suite_frust_plugin_runtime PUBLIC include)
target_link_libraries(creation_suite_frust_plugin_runtime PUBLIC frust_plugin_host)
