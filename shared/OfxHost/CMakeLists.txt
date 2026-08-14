cmake_minimum_required(VERSION 3.22)
project(CreationSuiteOfxHost LANGUAGES CXX)

add_library(creation_suite_ofxhost STATIC
    src/OfxPropertySet.cpp
    src/OfxHostSuites.cpp
)

target_include_directories(creation_suite_ofxhost PUBLIC
    include
    "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/openfx"
)
target_compile_features(creation_suite_ofxhost PUBLIC cxx_std_20)

add_executable(creation_suite_ofxhost_smoke
    tests/OfxHostSmoke.cpp
)

target_link_libraries(creation_suite_ofxhost_smoke
    PRIVATE
        creation_suite_ofxhost
)

add_test(
    NAME creation_suite_ofxhost_smoke
    COMMAND creation_suite_ofxhost_smoke
)
