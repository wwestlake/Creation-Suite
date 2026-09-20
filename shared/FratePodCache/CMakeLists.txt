cmake_minimum_required(VERSION 3.22)
project(CreationSuiteFratePodCache LANGUAGES CXX)

# The Suite's pod pipeline, entirely in memory over the VFS: pods are resolved
# from the VFS cache or the registry (FratePodVfsResolver), built without a
# folder or a child process (SuiteFratePodWorkspace, using frate::buildPod and
# the embeddable compiler in frust_lang), and stored as VFS entries.
#
# Depends on frate_lib (FrateRegistryClient, PodArchive) and frust_lang (the
# embeddable compiler and frate::buildPod) -- both brought in by
# shared/FrustPluginRuntime -- and creation_suite_asset_system
# (SuiteVfsServiceClient). Deliberately NOT added by shared/CMakeLists.txt
# unconditionally -- same reasoning as shared/NodeLibraryLoader: a
# consuming app's own CMakeLists.txt must add_subdirectory those (guarded the
# same way) before this one.
if(NOT TARGET frate_lib)
    message(FATAL_ERROR "FratePodCache requires frate_lib (add shared/FrustPluginRuntime first).")
endif()
if(NOT TARGET frust_lang)
    message(FATAL_ERROR "FratePodCache requires frust_lang (add shared/FrustPluginRuntime first).")
endif()
if(NOT TARGET creation_suite_asset_system)
    message(FATAL_ERROR "FratePodCache requires creation_suite_asset_system's SuiteVfsServiceClient to already exist.")
endif()

add_library(creation_suite_frate_pod_cache STATIC
    src/FratePodVfsResolver.cpp
    src/SuiteFratePodWorkspace.cpp
)

if(NOT TARGET frate_pod_cache)
    add_library(frate_pod_cache ALIAS creation_suite_frate_pod_cache)
endif()

target_include_directories(creation_suite_frate_pod_cache PUBLIC include)
target_compile_features(creation_suite_frate_pod_cache PUBLIC cxx_std_20)
target_link_libraries(creation_suite_frate_pod_cache
    PUBLIC
        frate_lib
        frust_lang
        creation_suite_asset_system
        juce::juce_core
)

add_executable(CreationSuiteFratePodCacheSmoke
    tests/FratePodVfsResolverSmoke.cpp
)
target_link_libraries(CreationSuiteFratePodCacheSmoke PRIVATE
    creation_suite_frate_pod_cache
)
add_test(NAME creation_suite_frate_pod_cache_smoke COMMAND CreationSuiteFratePodCacheSmoke)

add_executable(CreationSuiteFratePodWorkspaceSmoke
    tests/SuiteFratePodWorkspaceSmoke.cpp
)
target_link_libraries(CreationSuiteFratePodWorkspaceSmoke PRIVATE
    creation_suite_frate_pod_cache
)
add_test(NAME creation_suite_frate_pod_workspace_smoke COMMAND CreationSuiteFratePodWorkspaceSmoke)

# Auto-generated output name overrides
set_target_properties(creation_suite_frate_pod_cache PROPERTIES OUTPUT_NAME "djehuti_suite_frate_pod_cache")
set_target_properties(CreationSuiteFratePodCacheSmoke PROPERTIES OUTPUT_NAME "DjehutiSuiteFratePodCacheSmoke")
set_target_properties(CreationSuiteFratePodWorkspaceSmoke PROPERTIES OUTPUT_NAME "DjehutiSuiteFratePodWorkspaceSmoke")
