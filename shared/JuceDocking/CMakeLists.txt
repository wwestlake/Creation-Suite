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

# On Linux, JUCE modules are recompiled from source for every target that
# links them (no shared "libjuce" - each target gets its own translation
# units) - so juce_gui_extra.cpp gets compiled here too, and needs GTK3's
# headers, which live under a non-default include path
# (/usr/include/gtk-3.0/gtk/gtk.h) that installing libgtk-3-dev alone
# doesn't add to the compiler's default search. Not needed on Windows/Mac.
if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    target_include_directories(CreationDock PRIVATE ${GTK3_INCLUDE_DIRS})
endif()

target_compile_features(CreationDock PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(CreationDock PRIVATE /FS)
endif()
