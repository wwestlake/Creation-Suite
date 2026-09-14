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
target_include_directories(CreationDock PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(CreationDock PUBLIC
    juce::juce_core
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
)
target_compile_features(CreationDock PUBLIC cxx_std_20)

# The suite's documented build contract is single-worker on Windows. JUCE's
# generated projects may otherwise add /MP independently of MSBuild's /m:1,
# causing several cl.exe instances to contend for CreationDock.pdb and
# making the desktop host needlessly saturate the machine.
if(MSVC)
    target_compile_options(CreationDock PRIVATE /MP1 /FS)
endif()

# Keep this small shared library as one compiler translation unit on the
# Visual Studio generator. That makes the single-worker contract independent
# of JUCE/CMake's generated MultiProcessorCompilation setting and prevents
# concurrent writers to CreationDock.pdb.
set_target_properties(CreationDock PROPERTIES UNITY_BUILD ON UNITY_BUILD_BATCH_SIZE 1)
