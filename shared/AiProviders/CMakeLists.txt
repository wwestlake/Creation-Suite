# The AI provider adapters: each speaks one wire protocol behind the Provider interface (shared/AiCore). This one
# is the OpenAI chat-completions protocol, which OpenAI and many compatible providers use. Builds and tests on its
# own too: configure with -DJUCE_DIR=<path to a JUCE checkout>.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.22)
    project(creation_suite_ai_providers LANGUAGES C CXX)
    enable_testing()
    if(NOT DEFINED JUCE_DIR)
        message(FATAL_ERROR "Set -DJUCE_DIR to a JUCE checkout.")
    endif()
    add_subdirectory("${JUCE_DIR}" juce)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../AiCore" ai_core)
endif()

add_library(creation_suite_ai_providers STATIC
    include/creation/ai/HttpClient.h
    include/creation/ai/OpenAiWire.h
    include/creation/ai/OpenAiProvider.h
    src/HttpClient.cpp
    src/OpenAiWire.cpp
    src/OpenAiProvider.cpp
)
target_include_directories(creation_suite_ai_providers PUBLIC include)
target_compile_features(creation_suite_ai_providers PUBLIC cxx_std_20)
target_link_libraries(creation_suite_ai_providers PUBLIC creation_suite_ai_core juce::juce_core)
if(MSVC)
    target_compile_options(creation_suite_ai_providers PRIVATE /FS)
endif()
set_target_properties(creation_suite_ai_providers PROPERTIES OUTPUT_NAME "djehuti_suite_ai_providers")

add_executable(creation_suite_ai_openai_smoke tests/OpenAiSmoke.cpp)
target_link_libraries(creation_suite_ai_openai_smoke PRIVATE creation_suite_ai_providers)
add_test(NAME creation_suite_ai_openai_smoke COMMAND creation_suite_ai_openai_smoke)

add_executable(creation_suite_ai_http_smoke tests/HttpClientSmoke.cpp)
target_link_libraries(creation_suite_ai_http_smoke PRIVATE creation_suite_ai_providers)
add_test(NAME creation_suite_ai_http_smoke COMMAND creation_suite_ai_http_smoke)
