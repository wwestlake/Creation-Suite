add_library(creation_suite_ui STATIC
    src/CreationSuiteHeaderBar.cpp
    src/SuiteEulaPanel.cpp
    src/SuiteSettingsPanel.cpp
)

target_include_directories(creation_suite_ui PUBLIC include)
target_link_libraries(creation_suite_ui
    PUBLIC
        creation_suite_asset_system
        creation_suite_services
        juce::juce_gui_extra
)
target_compile_features(creation_suite_ui PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(creation_suite_ui PRIVATE /FS)
endif()
