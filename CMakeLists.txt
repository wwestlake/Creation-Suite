cmake_minimum_required(VERSION 3.22)

project(CreationSuiteShared LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

if(NOT DEFINED JUCE_DIR)
    message(FATAL_ERROR "Set JUCE_DIR to your JUCE root directory before configuring Creation Suite shared libraries.")
endif()

add_subdirectory("${JUCE_DIR}" juce)
add_subdirectory(shared)
