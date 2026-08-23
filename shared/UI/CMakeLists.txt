add_library(creation_suite_ui STATIC
    src/CreationSuiteLogos.cpp
    src/CreationSuiteHeaderBar.cpp
    src/SuiteCommonSpacePanel.cpp
    src/SuiteJUCEApplication.cpp
    src/SuiteDesktopAuthSession.cpp
    src/SuiteEulaPanel.cpp
    src/SuiteShellController.cpp
    src/SuiteSettingsPanel.cpp
    src/SuiteAiChatPanel.cpp
    src/SuiteAssetManagerPanel.cpp
    src/NamedAssetSaveLoadMenu.cpp
    src/SuiteAiAccountDialog.cpp
)

juce_add_binary_data(creation_suite_ui_assets
    SOURCES
        assets/creation_suite.png
        assets/creation_station.png
        assets/creation_engine.png
        assets/creation_engineer.png
        assets/creation_movie.png
        assets/creation_live.png
        assets/creation_texture.png
        assets/creation_modeler.png
        assets/creation_suite_banner.png
)

target_include_directories(creation_suite_ui PUBLIC include)
target_link_libraries(creation_suite_ui
    PUBLIC
        creation_suite_asset_system
        creation_suite_services
        creation_suite_ui_assets
        juce::juce_gui_extra
)
target_compile_features(creation_suite_ui PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(creation_suite_ui PRIVATE /FS)
endif()
