# Builds and tests on its own too: configure it with -DJUCE_DIR=<path to a JUCE checkout>.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.22)
    project(creation_suite_ai_core LANGUAGES C CXX)
    enable_testing()
    if(NOT DEFINED JUCE_DIR)
        message(FATAL_ERROR "Set -DJUCE_DIR to a JUCE checkout.")
    endif()
    add_subdirectory("${JUCE_DIR}" juce)
endif()

add_library(creation_suite_ai_core STATIC
    include/creation/ai/Messages.h
    include/creation/ai/Provider.h
    include/creation/ai/ScriptedProvider.h
    include/creation/ai/Tool.h
    include/creation/ai/ToolRegistry.h
    include/creation/ai/AgentEngine.h
    src/Messages.cpp
    src/ScriptedProvider.cpp
    src/ToolRegistry.cpp
    src/AgentEngine.cpp
)

target_include_directories(creation_suite_ai_core PUBLIC include)
target_link_libraries(creation_suite_ai_core PUBLIC juce::juce_core)
target_compile_features(creation_suite_ai_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(creation_suite_ai_core PRIVATE /FS)
endif()

set_target_properties(creation_suite_ai_core PROPERTIES OUTPUT_NAME "djehuti_suite_ai_core")

add_executable(creation_suite_ai_core_smoke tests/AiCoreSmoke.cpp)
target_link_libraries(creation_suite_ai_core_smoke PRIVATE creation_suite_ai_core)
add_test(NAME creation_suite_ai_core_smoke COMMAND creation_suite_ai_core_smoke)

add_executable(creation_suite_ai_tools_smoke tests/ToolRegistrySmoke.cpp)
target_link_libraries(creation_suite_ai_tools_smoke PRIVATE creation_suite_ai_core)
add_test(NAME creation_suite_ai_tools_smoke COMMAND creation_suite_ai_tools_smoke)

add_executable(creation_suite_ai_engine_smoke tests/AgentEngineSmoke.cpp)
target_link_libraries(creation_suite_ai_engine_smoke PRIVATE creation_suite_ai_core)
add_test(NAME creation_suite_ai_engine_smoke COMMAND creation_suite_ai_engine_smoke)
