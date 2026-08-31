cmake_minimum_required(VERSION 3.22)
project(CreationSuiteMaterialSystem LANGUAGES CXX)
enable_testing()

if(NOT TARGET creation_suite_node_system)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../NodeSystem" CreationSuiteMaterialSystemNodeSystem)
endif()

add_library(creation_suite_material_system STATIC
    src/material_nodes.cpp
    src/material_compiler.cpp
)

target_include_directories(creation_suite_material_system PUBLIC include)
target_link_libraries(creation_suite_material_system PUBLIC creation_suite_node_system)
target_compile_features(creation_suite_material_system PUBLIC cxx_std_20)

add_executable(creation_suite_material_system_smoke tests/MaterialSystemSmoke.cpp)
target_link_libraries(creation_suite_material_system_smoke PRIVATE creation_suite_material_system)
add_test(NAME creation_suite_material_system_smoke COMMAND creation_suite_material_system_smoke)
