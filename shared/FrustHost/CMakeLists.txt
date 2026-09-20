cmake_minimum_required(VERSION 3.22)
project(CreationSuiteFrustHost LANGUAGES CXX)

# The Suite's front end for the FRust libraries. The compiler and Frate (frust_lang, static)
# read, write and log only through a HostEnvironment; this library supplies one backed by the
# VFS (VfsFileSystem, VfsLogSink) and SuiteFrust, the single class an app calls.
#
# Needs frust_lang and frate_lib (brought in by shared/FrustPluginRuntime), the plugin runtime,
# and the asset system (SuiteVfsServiceClient, ProjectSession). A consuming app's own
# CMakeLists.txt must add those before this one.
foreach(required frate_lib frust_lang creation_suite_frust_plugin_runtime creation_suite_asset_system)
    if(NOT TARGET ${required})
        message(FATAL_ERROR "FrustHost requires the ${required} target to already exist.")
    endif()
endforeach()

add_library(creation_suite_frust_host STATIC
    src/VfsEnvironment.cpp
    src/SuiteFrust.cpp
)

if(NOT TARGET frust_host)
    add_library(frust_host ALIAS creation_suite_frust_host)
endif()

target_include_directories(creation_suite_frust_host PUBLIC include)
target_compile_features(creation_suite_frust_host PUBLIC cxx_std_20)
target_link_libraries(creation_suite_frust_host
    PUBLIC
        frate_lib
        frust_lang
        creation_suite_frust_plugin_runtime
        creation_suite_asset_system
        juce::juce_core
)

add_executable(CreationSuiteFrustHostSmoke tests/SuiteFrustSmoke.cpp)
target_link_libraries(CreationSuiteFrustHostSmoke PRIVATE creation_suite_frust_host)
add_test(NAME creation_suite_frust_host_smoke COMMAND CreationSuiteFrustHostSmoke)

set_target_properties(creation_suite_frust_host PROPERTIES OUTPUT_NAME "djehuti_suite_frust_host")
set_target_properties(CreationSuiteFrustHostSmoke PROPERTIES OUTPUT_NAME "DjehutiSuiteFrustHostSmoke")
