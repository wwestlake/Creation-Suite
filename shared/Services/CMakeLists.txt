add_library(creation_suite_services STATIC
    src/SuiteAiOrchestration.cpp
    src/SuiteAiProviderRuntime.cpp
    src/SuiteAiService.cpp
    src/SuiteLogging.cpp
    src/SuiteEula.cpp
    src/SuiteLegalSettings.cpp
    src/SuiteAiSettings.cpp
    src/SuiteProcessRegistry.cpp
    src/SuiteContextEngine.cpp
)

target_include_directories(creation_suite_services PUBLIC include)
target_link_libraries(creation_suite_services PUBLIC creation_suite_asset_system juce::juce_core)
target_compile_features(creation_suite_services PUBLIC cxx_std_20)

add_executable(creation_suite_services_smoke
    tests/ServicesSmoke.cpp
)

target_link_libraries(creation_suite_services_smoke
    PRIVATE
        creation_suite_services
)

add_test(
    NAME creation_suite_services_smoke
    COMMAND creation_suite_services_smoke
)
