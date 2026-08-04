add_library(creation_suite_services STATIC
    src/SuiteAiOrchestration.cpp
    src/SuiteAiProviderRuntime.cpp
    src/SuiteAiService.cpp
    src/SuiteLogging.cpp
    src/SuiteEula.cpp
    src/SuiteLegalSettings.cpp
    src/SuiteAiSettings.cpp
    src/SuiteProcessRegistry.cpp
    src/SuiteProjectHandoff.cpp
    src/SuiteVfsServiceClient.cpp
    src/SuiteContextEngine.cpp
    src/SuiteAiChatClient.cpp
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

# SuiteHandoffOwner is a helper process spawned by
# SuiteProjectHandoffMultiProcessSmoke, not a test in its own right.
add_executable(SuiteHandoffOwner
    tests/SuiteHandoffOwner.cpp
)

target_link_libraries(SuiteHandoffOwner
    PRIVATE
        creation_suite_services
)

add_executable(creation_suite_project_handoff_multiprocess_smoke
    tests/SuiteProjectHandoffMultiProcessSmoke.cpp
)

target_link_libraries(creation_suite_project_handoff_multiprocess_smoke
    PRIVATE
        creation_suite_services
)

add_dependencies(creation_suite_project_handoff_multiprocess_smoke SuiteHandoffOwner)

add_test(
    NAME creation_suite_project_handoff_multiprocess_smoke
    COMMAND creation_suite_project_handoff_multiprocess_smoke
)
