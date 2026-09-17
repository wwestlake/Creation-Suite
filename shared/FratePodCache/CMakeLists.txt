cmake_minimum_required(VERSION 3.22)
project(CreationSuiteFratePodCache LANGUAGES CXX)

# Depends on frate_lib (FrateRegistryClient -- brought in transitively by
# shared/FrustPluginRuntime, which already add_subdirectory's
# third_party/FrustLang/projects/05_frate) and creation_suite_asset_system
# (SuiteVfsServiceClient). Deliberately NOT added by shared/CMakeLists.txt
# unconditionally -- same reasoning as shared/NodeLibraryLoader: a
# consuming app's own CMakeLists.txt must add_subdirectory both of those
# (guarded the same way) before this one.
if(NOT TARGET frate_lib)
    message(FATAL_ERROR "FratePodCache requires frate_lib (add shared/FrustPluginRuntime first).")
endif()
if(NOT TARGET creation_suite_asset_system)
    message(FATAL_ERROR "FratePodCache requires creation_suite_asset_system's SuiteVfsServiceClient to already exist.")
endif()

add_library(creation_suite_frate_pod_cache STATIC
    src/FratePodVfsResolver.cpp
    src/SuiteFrateBuildService.cpp
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

if(NOT TARGET frate_cli)
    message(FATAL_ERROR "FratePodCache's build-service smoke test requires the frate_cli target (add shared/FrustPluginRuntime first, which pulls it in transitively).")
endif()

add_executable(CreationSuiteFrateBuildServiceSmoke
    tests/SuiteFrateBuildServiceSmoke.cpp
)
target_link_libraries(CreationSuiteFrateBuildServiceSmoke PRIVATE
    creation_suite_frate_pod_cache
)
# The real, already-built frate.exe from this same CMake tree -- not a
# guessed relative path, and not a second build of frate_cli.
target_compile_definitions(CreationSuiteFrateBuildServiceSmoke PRIVATE
    FRATE_EXECUTABLE_PATH="$<TARGET_FILE:frate_cli>"
)
add_dependencies(CreationSuiteFrateBuildServiceSmoke frate_cli)
add_test(NAME creation_suite_frate_build_service_smoke COMMAND CreationSuiteFrateBuildServiceSmoke)

add_executable(CreationSuiteFratePodWorkspaceSmoke
    tests/SuiteFratePodWorkspaceSmoke.cpp
)
target_link_libraries(CreationSuiteFratePodWorkspaceSmoke PRIVATE
    creation_suite_frate_pod_cache
)
target_compile_definitions(CreationSuiteFratePodWorkspaceSmoke PRIVATE
    FRATE_EXECUTABLE_PATH="$<TARGET_FILE:frate_cli>"
)
add_dependencies(CreationSuiteFratePodWorkspaceSmoke frate_cli)
add_test(NAME creation_suite_frate_pod_workspace_smoke COMMAND CreationSuiteFratePodWorkspaceSmoke)

# Auto-generated output name overrides
set_target_properties(creation_suite_frate_pod_cache PROPERTIES OUTPUT_NAME "djehuti_suite_frate_pod_cache")
set_target_properties(CreationSuiteFratePodCacheSmoke PROPERTIES OUTPUT_NAME "DjehutiSuiteFratePodCacheSmoke")
set_target_properties(CreationSuiteFratePodWorkspaceSmoke PROPERTIES OUTPUT_NAME "DjehutiSuiteFratePodWorkspaceSmoke")
set_target_properties(CreationSuiteFrateBuildServiceSmoke PROPERTIES OUTPUT_NAME "DjehutiSuiteFrateBuildServiceSmoke")
