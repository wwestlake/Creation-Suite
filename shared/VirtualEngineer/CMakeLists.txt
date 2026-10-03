add_library(creation_suite_virtual_engineer STATIC
    src/VirtualEngineer.cpp
    src/AgentApi.cpp
)

target_include_directories(creation_suite_virtual_engineer PUBLIC include)
target_link_libraries(creation_suite_virtual_engineer PUBLIC creation_suite_litesemrag juce::juce_events)
target_compile_features(creation_suite_virtual_engineer PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(creation_suite_virtual_engineer PRIVATE /FS)
endif()

add_executable(creation_suite_virtual_engineer_smoke
    tests/VirtualEngineerSmoke.cpp
)

target_link_libraries(creation_suite_virtual_engineer_smoke
    PRIVATE
        creation_suite_virtual_engineer
)

add_test(
    NAME creation_suite_virtual_engineer_smoke
    COMMAND creation_suite_virtual_engineer_smoke
)
