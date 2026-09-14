cmake_minimum_required(VERSION 3.22)

add_library(creation_suite_character_system STATIC
    src/CharacterSchemas.cpp
    src/CharacterAssetStore.cpp
)

target_include_directories(creation_suite_character_system PUBLIC include)
target_link_libraries(creation_suite_character_system PUBLIC creation_suite_asset_system juce::juce_data_structures)
target_compile_features(creation_suite_character_system PUBLIC cxx_std_20)

add_executable(creation_suite_character_system_smoke tests/CharacterSystemSmoke.cpp)
target_link_libraries(creation_suite_character_system_smoke PRIVATE creation_suite_character_system)

# This is a disposable command-line verification tool, not an editor binary.
# Avoid Visual Studio's incremental-link database so a stale .ilk cannot block
# a focused smoke-test build after an interrupted compile.
if(MSVC)
    target_link_options(creation_suite_character_system_smoke PRIVATE /INCREMENTAL:NO)
endif()

add_test(NAME creation_suite_character_system_smoke COMMAND creation_suite_character_system_smoke)
