add_library(CreationDock STATIC
    src/DockPanel.cpp
    src/DockTab.cpp
    src/DockTabStrip.cpp
    src/DockZone.cpp
    src/DockOverlay.cpp
    src/DockSplitter.cpp
    src/FloatingDockWindow.cpp
    src/DockContainer.cpp
    src/DockManager.cpp
)

target_compile_definitions(CreationDock PUBLIC JUCE_WEB_BROWSER=0)
target_include_directories(CreationDock PUBLIC include)
target_link_libraries(CreationDock PUBLIC
    juce::juce_core
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
)
target_compile_features(CreationDock PUBLIC cxx_std_20)
