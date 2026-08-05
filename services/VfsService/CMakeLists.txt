cmake_minimum_required(VERSION 3.22)

set(CREATION_SUITE_VFS_SERVICE_VERSION "0.0.1" CACHE STRING "Creation Suite VFS Service version")
project(CreationSuiteVfsService VERSION ${CREATION_SUITE_VFS_SERVICE_VERSION} LANGUAGES CXX)

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
    PRODUCT_NAME "Creation Suite VFS Service"
    COMPANY_NAME "LagDaemon Software"
    BUNDLE_ID "com.lagdaemon.creationsuite.vfsservice"
)

juce_generate_juce_header(CreationSuiteVfsService)

target_sources(CreationSuiteVfsService PRIVATE
    Source/Main.cpp
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

add_custom_command(TARGET CreationSuiteVfsService POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "D:/CreationSuite-Workspaces/codex-$<LOWER_CASE:$<CONFIG>>-bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:CreationSuiteVfsService>" "D:/CreationSuite-Workspaces/codex-$<LOWER_CASE:$<CONFIG>>-bin/"
    COMMAND cmd /c "if exist \"${CREATION_VFS_DLL_COPY_PREFIX}\\bin\\z.dll\" copy /Y \"${CREATION_VFS_DLL_COPY_PREFIX}\\bin\\z.dll\" \"D:\\CreationSuite-Workspaces\\codex-$<LOWER_CASE:$<CONFIG>>-bin\\\" >nul"
    COMMAND cmd /c "if exist \"${CREATION_VFS_DLL_COPY_PREFIX}\\debug\\bin\\zd.dll\" copy /Y \"${CREATION_VFS_DLL_COPY_PREFIX}\\debug\\bin\\zd.dll\" \"D:\\CreationSuite-Workspaces\\codex-$<LOWER_CASE:$<CONFIG>>-bin\\\" >nul"
    COMMENT "Copying CreationSuiteVfsService (and its runtime DLLs) to the shared codex bin directory"
)

install(TARGETS CreationSuiteVfsService
    RUNTIME DESTINATION .
)
