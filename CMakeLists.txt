cmake_minimum_required(VERSION 3.22)

project(CreationSuiteShared LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include("${CMAKE_CURRENT_SOURCE_DIR}/shared/CMake/CreationSuiteBuildSettings.cmake")
creation_suite_require_juce()

enable_testing()

add_subdirectory("${JUCE_DIR}" juce)
add_subdirectory(shared)
