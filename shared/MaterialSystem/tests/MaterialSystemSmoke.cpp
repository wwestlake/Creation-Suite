#include <creation/material/material_compiler.h>
#include <creation/material/material_nodes.h>

#include <iostream>
#include <stdexcept>

int main()
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
    std::cout << "MaterialSystem smoke passed.\n";
}
