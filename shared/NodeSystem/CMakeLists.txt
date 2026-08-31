cmake_minimum_required(VERSION 3.22)
project(CreationSuiteNodeSystem LANGUAGES CXX)

add_library(creation_suite_node_system STATIC
    src/node.cpp
    src/graph.cpp
    src/type_registry.cpp
    src/graph_analysis.cpp
    src/core_control_flow.cpp
    src/monad_nodes.cpp
    src/frgraph_serialization.cpp
)

if(NOT TARGET node_system)
    add_library(node_system ALIAS creation_suite_node_system)
endif()

target_include_directories(creation_suite_node_system PUBLIC include)
target_compile_features(creation_suite_node_system PUBLIC cxx_std_20)

add_executable(creation_suite_node_system_smoke
    tests/NodeSystemSmoke.cpp
)

target_link_libraries(creation_suite_node_system_smoke
    PRIVATE
        creation_suite_node_system
)

add_test(
    NAME creation_suite_node_system_smoke
    COMMAND creation_suite_node_system_smoke
)
