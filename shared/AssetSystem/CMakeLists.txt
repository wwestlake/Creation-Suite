add_library(creation_suite_asset_system STATIC
    src/AssetTypes.cpp
    src/AssetCatalog.cpp
    src/AssetMaterializer.cpp
    src/AssetResolver.cpp
    src/ProjectContainerIO.cpp
    src/ProjectManifest.cpp
    src/SuiteSettings.cpp
    src/SuiteStoragePaths.cpp
    src/VirtualFileSystem.cpp
)

target_include_directories(creation_suite_asset_system PUBLIC include)
target_link_libraries(creation_suite_asset_system PUBLIC juce::juce_core)
target_compile_features(creation_suite_asset_system PUBLIC cxx_std_20)

add_executable(creation_suite_asset_system_smoke
    tests/AssetSystemSmoke.cpp
)

target_link_libraries(creation_suite_asset_system_smoke
    PRIVATE
        creation_suite_asset_system
        juce::juce_core
)

add_test(
    NAME creation_suite_asset_system_smoke
    COMMAND creation_suite_asset_system_smoke
)
