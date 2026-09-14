# Creation Suite Material System

The Material System is the typed graph-to-GLSL boundary for authored
materials. It shares `NodeSystem::Graph` and the Suite node editor, but its
backend is GLSL rather than FRust because the result executes on the GPU.

The first contract includes UV0, float and color constants, exposed float and
color parameters, float arithmetic, 2D texture sampling, and one explicit
Surface Output node. `CompileMaterialGraph` emits a deterministic
`EvaluateMaterial` function plus uniform declarations. The Engine owns the
fragment shader, lighting model, resource bindings, and OpenGL compilation;
the generated function supplies the material values to that host shader.
