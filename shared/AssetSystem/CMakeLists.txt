add_library(creation_suite_asset_system STATIC
    src/AssetTypes.cpp
    src/SuiteSettings.cpp
    src/VirtualFileSystem.cpp
)

target_include_directories(creation_suite_asset_system PUBLIC include)
target_link_libraries(creation_suite_asset_system PUBLIC juce::juce_core)
target_compile_features(creation_suite_asset_system PUBLIC cxx_std_20)
