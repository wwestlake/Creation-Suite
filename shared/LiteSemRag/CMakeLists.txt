add_library(creation_suite_litesemrag STATIC
    src/Cards.cpp
    src/CardStore.cpp
    src/Retrieval.cpp
    src/Embeddings.cpp
)

target_include_directories(creation_suite_litesemrag PUBLIC include)
target_link_libraries(creation_suite_litesemrag PUBLIC creation_suite_services juce::juce_core)
target_compile_features(creation_suite_litesemrag PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(creation_suite_litesemrag PRIVATE /FS)
endif()

add_executable(creation_suite_litesemrag_smoke
    tests/LiteSemRagSmoke.cpp
)

target_link_libraries(creation_suite_litesemrag_smoke
    PRIVATE
        creation_suite_litesemrag
)

add_test(
    NAME creation_suite_litesemrag_smoke
    COMMAND creation_suite_litesemrag_smoke
)
