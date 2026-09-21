# Audio analysis for the assistants and tools: measuring what is in a recording. Pure C++, no dependencies, so it also
# builds and tests on its own.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.22)
    project(creation_suite_audio_analysis LANGUAGES CXX)
    enable_testing()
endif()

add_library(creation_suite_audio_analysis STATIC
    include/creation/audio/PitchAnalysis.h
    include/creation/audio/TempoAnalysis.h
    include/creation/audio/DrumAnalysis.h
    src/DrumAnalysis.cpp
    src/PitchAnalysis.cpp
    src/PitchReport.cpp
    src/TempoAnalysis.cpp
)
target_include_directories(creation_suite_audio_analysis PUBLIC include)
target_compile_features(creation_suite_audio_analysis PUBLIC cxx_std_20)
set_target_properties(creation_suite_audio_analysis PROPERTIES OUTPUT_NAME "djehuti_suite_audio_analysis")

add_executable(creation_suite_pitch_smoke tests/PitchAnalysisSmoke.cpp)
target_link_libraries(creation_suite_pitch_smoke PRIVATE creation_suite_audio_analysis)
add_test(NAME creation_suite_pitch_smoke COMMAND creation_suite_pitch_smoke)

add_executable(creation_suite_tempo_smoke tests/TempoAnalysisSmoke.cpp)
target_link_libraries(creation_suite_tempo_smoke PRIVATE creation_suite_audio_analysis)
add_test(NAME creation_suite_tempo_smoke COMMAND creation_suite_tempo_smoke)

add_executable(creation_suite_drum_smoke tests/DrumAnalysisSmoke.cpp)
target_link_libraries(creation_suite_drum_smoke PRIVATE creation_suite_audio_analysis)
add_test(NAME creation_suite_drum_smoke COMMAND creation_suite_drum_smoke)
