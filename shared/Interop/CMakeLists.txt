add_library(creation_suite_interop STATIC
    src/ProjectRegistry.cpp
)

target_include_directories(creation_suite_interop PUBLIC include)
target_link_libraries(creation_suite_interop PUBLIC creation_suite_asset_system juce::juce_core)
target_compile_features(creation_suite_interop PUBLIC cxx_std_20)

add_executable(creation_suite_interop_smoke
    tests/ProjectRegistrySmoke.cpp
)

target_link_libraries(creation_suite_interop_smoke
    PRIVATE
        creation_suite_interop
)

add_test(
    NAME creation_suite_interop_smoke
    COMMAND creation_suite_interop_smoke
)
