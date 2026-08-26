cmake_minimum_required(VERSION 3.22)
project(CreationSuiteVideo LANGUAGES CXX)

if(NOT TARGET node_system)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../NodeSystem" CreationSuiteVideoNodeSystem)
endif()

add_library(creation_suite_video STATIC
    src/ShaderCompileHelpers.cpp
    src/VideoRenderTargetPool.cpp
    src/ColorBalanceNode.cpp
    src/GaussianBlurNode.cpp
    src/TransformCropNode.cpp
    src/NativeVideoNodeRegistry.cpp
    src/NativeVideoGraphExecutor.cpp
)

# The two headers explicitly required at shared/include/video/ (INativeVideoNode, VideoFrameContext)
# sit outside this library's own directory - see that directory's own contents. Everything else
# native-video-specific follows the repo's normal per-library include/creation/<lib>/ shape.
target_include_directories(creation_suite_video PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../include"
    include
)

target_link_libraries(creation_suite_video PUBLIC node_system d3d11 dxgi d3dcompiler)
target_compile_features(creation_suite_video PUBLIC cxx_std_20)

if(WIN32)
    add_executable(creation_suite_video_smoke
        tests/VideoSmoke.cpp
    )

    target_link_libraries(creation_suite_video_smoke
        PRIVATE
            creation_suite_video
    )

    add_test(
        NAME creation_suite_video_smoke
        COMMAND creation_suite_video_smoke
    )
endif()
