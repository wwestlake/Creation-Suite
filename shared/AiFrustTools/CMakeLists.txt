cmake_minimum_required(VERSION 3.22)
project(CreationSuiteAiFrustTools LANGUAGES CXX)

# The tools that let an AI work as a coder in FRust (run_frust, check_frust, frust_api_reference). Sits between
# the agent engine (shared/AiCore, which knows nothing about FRust) and the script runner
# (shared/FrustPluginRuntime, which knows nothing about AI). A consuming app's CMakeLists.txt must add those
# two before this one.
foreach(required creation_suite_ai_core creation_suite_frust_plugin_runtime frust_lang)
    if(NOT TARGET ${required})
        message(FATAL_ERROR "AiFrustTools requires the ${required} target to already exist.")
    endif()
endforeach()

add_library(creation_suite_ai_frust_tools STATIC
    include/creation/ai/FrustTools.h
    src/FrustTools.cpp
)
target_include_directories(creation_suite_ai_frust_tools PUBLIC include)
target_compile_features(creation_suite_ai_frust_tools PUBLIC cxx_std_20)
target_link_libraries(creation_suite_ai_frust_tools
    PUBLIC
        creation_suite_ai_core
        creation_suite_frust_plugin_runtime
        frust_lang
        juce::juce_core
)
set_target_properties(creation_suite_ai_frust_tools PROPERTIES OUTPUT_NAME "djehuti_suite_ai_frust_tools")

add_executable(creation_suite_ai_frust_smoke tests/AiFrustSmoke.cpp)
target_link_libraries(creation_suite_ai_frust_smoke PRIVATE creation_suite_ai_frust_tools)
set_target_properties(creation_suite_ai_frust_smoke PROPERTIES OUTPUT_NAME "DjehutiSuiteAiFrustSmoke")
add_test(NAME creation_suite_ai_frust_smoke COMMAND creation_suite_ai_frust_smoke)
