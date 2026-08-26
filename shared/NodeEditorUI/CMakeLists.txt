cmake_minimum_required(VERSION 3.22)
project(CreationSuiteNodeEditorUI LANGUAGES CXX)

if(NOT TARGET node_system)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../NodeSystem" CreationSuiteNodeEditorUINodeSystem)
endif()

add_library(creation_suite_node_editor_ui STATIC
    src/NodeGraphComponent.cpp
    src/NodeInspector.cpp
    src/NodePalette.cpp
)

target_include_directories(creation_suite_node_editor_ui PUBLIC include)
target_link_libraries(creation_suite_node_editor_ui PUBLIC node_system juce::juce_gui_basics)
target_compile_features(creation_suite_node_editor_ui PUBLIC cxx_std_20)
