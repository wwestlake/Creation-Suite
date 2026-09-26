#include <creation/material/material_compiler.h>
#include <creation/material/material_nodes.h>

#include <iostream>
#include <stdexcept>

namespace
{
void RunSmoke()
{
    ce::node_system::NodeTypeRegistry registry;
    ce::material::RegisterMaterialNodes(registry);
    ce::node_system::Graph graph("MaterialSmoke");
    std::string error;
    auto* color = ce::node_system::AddRegisteredNode(graph, registry, "material.constant.color", &error);
    auto* output = ce::node_system::AddRegisteredNode(graph, registry, "material.surface.output", &error);
    if (color == nullptr || output == nullptr) throw std::runtime_error("Failed to construct material nodes.");
    if (!graph.Connect(color->Id(), color->Outputs().front().id, output->Id(), output->Inputs().front().id).has_value())
        throw std::runtime_error("Failed to connect material color.");

    const auto compiled = ce::material::CompileMaterialGraph(graph, registry);
    if (!compiled.ok) throw std::runtime_error("Material compilation failed.");
    if (compiled.source.evaluateFunction.find("EvaluateMaterial") == std::string::npos)
        throw std::runtime_error("Generated material function is missing.");
    if (compiled.source.evaluateFunction.find("vec3(1.0f") == std::string::npos)
        throw std::runtime_error("Generated color literal is missing.");
    // Texture Sample: the generated uniform must be a legal GLSL identifier. It used to be
    // "uMaterialTex_" + "_0_<path>", and GLSL reserves any identifier containing "__", so every textured
    // material failed to compile on the GPU.
    {
        ce::node_system::Graph textured("TexturedSmoke");
        auto* sample = ce::node_system::AddRegisteredNode(textured, registry, "material.texture.sample2d", &error);
        auto* texturedOutput = ce::node_system::AddRegisteredNode(textured, registry, "material.surface.output", &error);
        if (sample == nullptr || texturedOutput == nullptr) throw std::runtime_error("Failed to construct texture nodes.");
        const std::string path = "Assets/Source/abstract-antique-backdrop-background-164005-seamless-color.png";
        for (const auto& pin : sample->Inputs())
            if (pin.name == "texture") sample->FindPin(pin.id)->defaultValue = path;
        if (!textured.Connect(sample->Id(), sample->Outputs().front().id, texturedOutput->Id(), texturedOutput->Inputs().front().id).has_value())
            throw std::runtime_error("Failed to connect texture sample.");

        const auto texturedCompiled = ce::material::CompileMaterialGraph(textured, registry);
        if (!texturedCompiled.ok) throw std::runtime_error("Textured material compilation failed.");
        const std::string glsl = texturedCompiled.source.declarations + texturedCompiled.source.evaluateFunction;
        if (glsl.find("__") != std::string::npos)
            throw std::runtime_error("Generated GLSL contains a reserved '__' identifier:\n" + glsl);
        if (texturedCompiled.source.textures.size() != 1 || texturedCompiled.source.textures.front().uniformName != "uMaterialTex0"
            || texturedCompiled.source.textures.front().path != path)
            throw std::runtime_error("Texture slot should be uMaterialTex0 for the sampled path.");
        if (glsl.find("uniform sampler2D uMaterialTex0;") == std::string::npos
            || texturedCompiled.source.evaluateFunction.find("texture(uMaterialTex0, vUV).rgb") == std::string::npos)
            throw std::runtime_error("Texture sampler declaration or sample expression is missing:\n" + glsl);
    }

    std::cout << "MaterialSystem smoke passed.\n";
}
}

// Failures print and return 1: an uncaught exception would raise a blocking Windows abort dialog.
int main()
{
    try {
        RunSmoke();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "MaterialSystem smoke FAILED: " << e.what() << "\n";
        return 1;
    }
}
