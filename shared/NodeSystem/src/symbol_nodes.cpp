#include "node_system/symbol_nodes.h"

namespace ce::node_system {

namespace {
struct GetType {
    DataType type;
    const char* suffix;
    const char* displayName;
};

const GetType kGetTypes[] = {
    { DataType::Float, "float", "Get Number" },
    { DataType::Int, "int", "Get Integer" },
    { DataType::Bool, "bool", "Get Toggle" },
    { DataType::Color, "color", "Get Color" },
    { DataType::Vec3, "vec3", "Get Vector" },
    { DataType::String, "string", "Get Text" },
};

constexpr const char* kPrefix = "core.symbol.get.";
} // namespace

void RegisterSymbolGetNodes(NodeTypeRegistry& registry) {
    for (const auto& get : kGetTypes) {
        NodeTypeDescriptor d;
        d.typeName = std::string(kPrefix) + get.suffix;
        d.domain = Domain::Core;
        d.inputs = { { kSymbolIdPin, { PinKind::Data, DataType::String }, std::string() } };
        d.outputs = { { kSymbolValuePin, { PinKind::Data, get.type }, {} } };
        d.displayName = get.displayName;
        d.category = "Symbols";
        d.description = "Reads one of the graph's params, constants or variables (shared/NodeSystem/SYMBOLS.md).";
        registry.Register(std::move(d));
    }
}

std::string SymbolGetNodeType(DataType type) {
    for (const auto& get : kGetTypes)
        if (get.type == type)
            return std::string(kPrefix) + get.suffix;
    return {};
}

bool IsSymbolGetNode(const std::string& typeName) {
    return typeName.rfind(kPrefix, 0) == 0;
}

const Symbol* SymbolForGetNode(const Graph& graph, const Node& node) {
    if (!IsSymbolGetNode(node.TypeName()))
        return nullptr;
    for (const auto& pin : node.Inputs())
        if (pin.name == kSymbolIdPin)
            if (const auto* id = std::get_if<std::string>(&pin.defaultValue))
                return graph.FindSymbol(*id);
    return nullptr;
}

void BindSymbolGetNode(Node& node, const Symbol& symbol) {
    for (const auto& pin : node.Inputs())
        if (pin.name == kSymbolIdPin)
            node.FindPin(pin.id)->defaultValue = symbol.id;
    for (const auto& pin : node.Outputs())
        if (pin.name == kSymbolValuePin)
            node.FindPin(pin.id)->type.enumType = symbol.enumType;
}

Node* AddSymbolGetNode(Graph& graph, const NodeTypeRegistry& registry, const Symbol& symbol, std::string* errorOut) {
    const auto type = SymbolGetNodeType(symbol.type);
    if (type.empty()) {
        if (errorOut != nullptr)
            *errorOut = "Symbols of this type have no Get node.";
        return nullptr;
    }
    Node* node = AddRegisteredNode(graph, registry, type, errorOut);
    if (node == nullptr)
        return nullptr;
    BindSymbolGetNode(*node, symbol);
    return node;
}

} // namespace ce::node_system
