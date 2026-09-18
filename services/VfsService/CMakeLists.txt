cmake_minimum_required(VERSION 3.22)

set(CREATION_SUITE_VFS_SERVICE_VERSION "0.0.1" CACHE STRING "Djehuti Suite VFS Service version")
project(CreationSuiteVfsService VERSION ${CREATION_SUITE_VFS_SERVICE_VERSION} LANGUAGES CXX)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../shared/CMake/CreationSuiteBuildSettings.cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED JUCE_DIR)
    message(FATAL_ERROR "Set JUCE_DIR to your JUCE root directory before configuring (same JUCE checkout the other Creation apps use).")
endif()

find_package(httplib CONFIG REQUIRED)
find_package(ixwebsocket CONFIG REQUIRED)

add_subdirectory("${JUCE_DIR}" juce)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../shared/VFS" CreationSuiteSharedVFS)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../shared/AssetSystem" CreationSuiteSharedAssetSystem)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../shared/Services" CreationSuiteSharedServices)

juce_add_console_app(CreationSuiteVfsService
    PRODUCT_NAME "DjehutiSuiteVFSService"
    COMPANY_NAME "LagDaemon Software"
    BUNDLE_ID "com.lagdaemon.creationsuite.vfsservice"
)

juce_generate_juce_header(CreationSuiteVfsService)

target_sources(CreationSuiteVfsService PRIVATE
    Source/Main.cpp
    Source/VfsProjectStore.cpp
)

target_compile_definitions(CreationSuiteVfsService PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    # vcpkg's zlib (pulled in by ixwebsocket for permessage-deflate) would
    # otherwise collide with JUCE's own bundled zlib symbols at link time
    # (both compiled into this target's juce_core.obj) -- tell JUCE to use
    # the external one instead of vendoring its own.
    JUCE_INCLUDE_ZLIB_CODE=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:CreationSuiteVfsService,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:CreationSuiteVfsService,JUCE_VERSION>"
)

# Keep this long-lived local service build predictable on Windows. MSVC's
# /MP fan-out can race its own generated objects and PDBs when other suite
# builds are active; the outer build already requests one worker as well.
if(MSVC)
    target_compile_options(CreationSuiteVfsService PRIVATE /MP1 /FS)
endif()

find_package(ZLIB REQUIRED)

set(CREATION_VFS_DLL_COPY_PREFIX "")
if(CMAKE_PREFIX_PATH)
    list(GET CMAKE_PREFIX_PATH 0 CREATION_VFS_DLL_COPY_PREFIX)
endif()

target_link_libraries(CreationSuiteVfsService PRIVATE
    creation_suite_asset_system
    creation_suite_services
    httplib::httplib
    ixwebsocket::ixwebsocket
    ZLIB::ZLIB
    juce::juce_core
    juce::juce_events
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

creation_suite_get_shared_bin_dir(CREATION_VFS_SHARED_BIN_DIR)

# Was hardcoded to "codex-<config>-bin" regardless of which agent built it --
# every other suite target uses this same dynamic per-agent helper (see
# CreationSuiteBuildSettings.cmake's creation_suite_get_shared_bin_dir/
# creation_suite_add_shared_bin_copy), which resolves the agent id from the
# checkout's own directory name (CreationSuite-<Agent>). A hardcoded
# destination here meant every agent's build silently overwrote (or, once
# that destination was locked by another agent's running process, failed to
# overwrite) Codex's copy instead of producing its own -- and a client built
# against a *different* agent's already-stale VfsService binary is exactly
# what caused Djehuti Station to hang on startup after the AppData folder
# rename (the client's rebuilt code expected the new folder name; the old
# service binary still assumed the old one).
add_custom_command(TARGET CreationSuiteVfsService POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CREATION_VFS_SHARED_BIN_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:CreationSuiteVfsService>" "${CREATION_VFS_SHARED_BIN_DIR}/"
    COMMAND cmd /c "if exist \"${CREATION_VFS_DLL_COPY_PREFIX}\\bin\\z.dll\" copy /Y \"${CREATION_VFS_DLL_COPY_PREFIX}\\bin\\z.dll\" \"${CREATION_VFS_SHARED_BIN_DIR}\\\" >nul"
    COMMAND cmd /c "if exist \"${CREATION_VFS_DLL_COPY_PREFIX}\\debug\\bin\\zd.dll\" copy /Y \"${CREATION_VFS_DLL_COPY_PREFIX}\\debug\\bin\\zd.dll\" \"${CREATION_VFS_SHARED_BIN_DIR}\\\" >nul"
    COMMENT "Copying CreationSuiteVfsService (and its runtime DLLs) to the shared per-agent bin directory"
)

install(TARGETS CreationSuiteVfsService
    RUNTIME DESTINATION .
)

set_target_properties(CreationSuiteVfsService PROPERTIES OUTPUT_NAME "DjehutiSuiteVfsService")
