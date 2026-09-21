# The assistants' own storage: where an assistant keeps what it writes, and the tamper-evident conversation ledger. Pure C++,
# no dependencies (an application backs AssistantFiles with its project storage), so it also builds and tests on its own.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.22)
    project(creation_suite_assistant_store LANGUAGES CXX)
    enable_testing()
endif()

add_library(creation_suite_assistant_store STATIC
    include/creation/assistant/AssistantFiles.h
    include/creation/assistant/ConversationLedger.h
    src/ConversationLedger.cpp
    src/ConversationStore.cpp
    src/MiniJson.cpp
    src/MiniJson.h
    src/Sha256.cpp
    src/Sha256.h
)
target_include_directories(creation_suite_assistant_store PUBLIC include)
target_compile_features(creation_suite_assistant_store PUBLIC cxx_std_20)
set_target_properties(creation_suite_assistant_store PROPERTIES OUTPUT_NAME "djehuti_suite_assistant_store")

add_executable(creation_suite_conversation_ledger_smoke tests/ConversationLedgerSmoke.cpp)
target_link_libraries(creation_suite_conversation_ledger_smoke PRIVATE creation_suite_assistant_store)
target_include_directories(creation_suite_conversation_ledger_smoke PRIVATE src)
add_test(NAME creation_suite_conversation_ledger_smoke COMMAND creation_suite_conversation_ledger_smoke)
