add_library(creation_suite_timeline STATIC
    src/TimelineEditOps.cpp
)

target_include_directories(creation_suite_timeline PUBLIC include)
target_link_libraries(creation_suite_timeline PUBLIC juce::juce_core creation_suite_asset_system)
target_compile_features(creation_suite_timeline PUBLIC cxx_std_20)
