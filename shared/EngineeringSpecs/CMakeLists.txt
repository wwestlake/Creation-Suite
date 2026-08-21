include("${CMAKE_CURRENT_LIST_DIR}/../CMake/CreationSuiteBuildSettings.cmake")
creation_suite_init_build_settings()

add_library(creation_suite_engineering_specs STATIC
    src/SpecTypes.cpp
    src/MaterialSpec.cpp
    src/ProfileSpec.cpp
    src/ConnectorSpec.cpp
    src/SpecLibrary.cpp
    src/BuiltinSpecLoader.cpp
)

target_include_directories(creation_suite_engineering_specs PUBLIC include)
target_link_libraries(creation_suite_engineering_specs PUBLIC juce::juce_core)
target_compile_features(creation_suite_engineering_specs PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(creation_suite_engineering_specs PRIVATE /FS)
endif()

add_executable(creation_suite_engineering_specs_smoke
    tests/EngineeringSpecsSmoke.cpp
)

target_link_libraries(creation_suite_engineering_specs_smoke
    PRIVATE
        creation_suite_engineering_specs
        juce::juce_core
)

target_compile_definitions(creation_suite_engineering_specs_smoke PRIVATE
    ENGINEERING_SPECS_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

add_test(
    NAME creation_suite_engineering_specs_smoke
    COMMAND creation_suite_engineering_specs_smoke
)
