add_library(creation_suite_render STATIC
    GL/Buffer.cpp
    GL/VertexArray.cpp
    Scene/Camera.cpp
    Scene/CursorGizmoRenderer.cpp
    Scene/FreeCamera.cpp
    Scene/GridRenderer.cpp
    Scene/Mesh.cpp
    Scene/ProceduralMesh.cpp
    Shaders/ShaderComposer.cpp
)

# Consumers #include via "Render/Scene/Camera.h" etc (the convention already
# used inside these files' own includes, e.g. Mesh.h's "Render/GL/Buffer.h") --
# exposing this directory's parent (shared/) as the public include root keeps
# every existing include statement working unchanged, in both this library
# and apps/CreationEngine's ViewportComponent.cpp, which used the same
# "Render/..." include convention before these files moved here.
target_include_directories(creation_suite_render PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(creation_suite_render PUBLIC
    creation_suite_asset_system
    juce::juce_core
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_opengl
)

target_compile_features(creation_suite_render PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(creation_suite_render PRIVATE /FS)
endif()
