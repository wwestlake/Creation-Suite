cmake_minimum_required(VERSION 3.22)
project(CreationSuiteNodeLibraryLoader LANGUAGES CXX)

# Depends on both node_system and the Frust plugin runtime -- deliberately
# NOT added by shared/CMakeLists.txt unconditionally, matching how
# shared/FrustPluginRuntime itself is only pulled in by apps that actually
# host Frust plugins. A consuming app's own CMakeLists.txt must
# add_subdirectory both of those (guarded the same way) before this one.
if(NOT TARGET node_system)
    message(FATAL_ERROR "NodeLibraryLoader requires shared/NodeSystem's node_system target to already exist.")
endif()
if(NOT TARGET creation_suite_frust_plugin_runtime)
    message(FATAL_ERROR "NodeLibraryLoader requires shared/FrustPluginRuntime's creation_suite_frust_plugin_runtime target to already exist.")
endif()

add_library(creation_suite_node_library_loader STATIC
    src/NodeLibraryLoader.cpp
)

if(NOT TARGET node_library_loader)
    add_library(node_library_loader ALIAS creation_suite_node_library_loader)
endif()

target_include_directories(creation_suite_node_library_loader PUBLIC include)
target_compile_features(creation_suite_node_library_loader PUBLIC cxx_std_20)
target_link_libraries(creation_suite_node_library_loader
    PUBLIC
        node_system
        creation_suite_frust_plugin_runtime
        juce::juce_core
)

# Auto-generated output name overrides
set_target_properties(creation_suite_node_library_loader PROPERTIES OUTPUT_NAME "djehuti_suite_node_library_loader")
