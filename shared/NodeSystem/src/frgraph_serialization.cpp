#include "node_system/frgraph_serialization.h"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <optional>
#include <sstream>
#include <type_traits>
#include <unordered_map>

namespace ce::node_system {

namespace {

std::string DomainToString(Domain d) {
    switch (d) {
        case Domain::Core: return "core";
        case Domain::Animation: return "animation";
        case Domain::Material: return "material";
        case Domain::Event: return "event";
        case Domain::Audio: return "audio";
        case Domain::Input: return "input";
    }
    return "core";
}

std::optional<Domain> DomainFromString(const std::string& s) {
    if (s == "core") return Domain::Core;
    if (s == "animation") return Domain::Animation;
    if (s == "material") return Domain::Material;
    if (s == "event") return Domain::Event;
    if (s == "audio") return Domain::Audio;
    if (s == "input") return Domain::Input;
    return std::nullopt;
}

std::string GraphTargetToString(GraphTarget target) {
    switch (target) {
        case GraphTarget::Behavior: return "behavior";
        case GraphTarget::Material: return "material";
        case GraphTarget::Dataflow: return "dataflow";
    }
    return "behavior";
}

std::optional<GraphTarget> GraphTargetFromString(const std::string& text) {
    if (text == "behavior") return GraphTarget::Behavior;
    if (text == "material") return GraphTarget::Material;
    if (text == "dataflow") return GraphTarget::Dataflow;
    return std::nullopt;
}

std::string DataTypeToString(DataType t) {
    switch (t) {
        case DataType::Float: return "float";
        case DataType::Vec2: return "vec2";
        case DataType::Vec3: return "vec3";
        case DataType::Vec4: return "vec4";
        case DataType::Color: return "color";
        case DataType::Bool: return "bool";
        case DataType::Int: return "int";
        case DataType::String: return "string";
        case DataType::Transform: return "transform";
        case DataType::BoneTransform: return "bonetransform";
        case DataType::Texture: return "texture";
        case DataType::AudioSignal: return "audiosignal";
        case DataType::Entity: return "entity";
        case DataType::Drawing: return "drawing";
        case DataType::Brush: return "brush";
        // These were missing and silently saved as "float", so they came back as Float pins (FLOW.md's selector is Any).
        case DataType::Any: return "any";
        case DataType::Function: return "function";
        case DataType::Material: return "material";
        case DataType::Model: return "model";
        case DataType::Controller: return "controller";
        case DataType::Struct: return "struct";
    }
    return "float";
}

std::optional<DataType> DataTypeFromString(const std::string& s) {
    static const std::unordered_map<std::string, DataType> table = {
        { "float", DataType::Float },     { "vec2", DataType::Vec2 },       { "vec3", DataType::Vec3 },
        { "vec4", DataType::Vec4 },       { "color", DataType::Color },     { "bool", DataType::Bool },
        { "int", DataType::Int },         { "string", DataType::String },   { "transform", DataType::Transform },
        { "bonetransform", DataType::BoneTransform }, { "texture", DataType::Texture },
        { "audiosignal", DataType::AudioSignal }, { "entity", DataType::Entity },
        { "drawing", DataType::Drawing }, { "brush", DataType::Brush }, { "any", DataType::Any },
        { "function", DataType::Function }, { "material", DataType::Material }, { "model", DataType::Model },
        { "controller", DataType::Controller }, { "struct", DataType::Struct },
    };
    const auto it = table.find(s);
    return it == table.end() ? std::nullopt : std::optional<DataType>(it->second);
}

std::string SerializeDefaultValue(const PinDefaultValue& value) {
    return std::visit(
        [](auto&& v) -> std::string {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                return "";
            } else if constexpr (std::is_same_v<T, float>) {
                std::ostringstream os;
                os << " default float " << v;
                return os.str();
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                return " default int " + std::to_string(v);
            } else if constexpr (std::is_same_v<T, bool>) {
                return std::string(" default bool ") + (v ? "true" : "false");
            } else if constexpr (std::is_same_v<T, std::string>) {
                // Rest-of-line -- must be the LAST field on its pin line (enforced by
                // being the only default kind that isn't followed by anything else).
                return " default string " + v;
            } else if constexpr (std::is_same_v<T, Vec3Default>) {
                std::ostringstream os;
                os << " default vec3 " << v.x << " " << v.y << " " << v.z;
                return os.str();
            }
        },
        value);
}

std::string SerializePinLine(NodeId nodeId, const Pin& pin) {
    std::ostringstream os;
    os << std::setprecision(9);
    os << "pin " << nodeId << " " << (pin.isInput ? "in" : "out") << " " << pin.id << " " << pin.name << " ";
    if (pin.type.kind == PinKind::Exec) {
        os << "exec";
    } else if (pin.type.kind == PinKind::Stream) {
        os << "stream " << DataTypeToString(pin.type.dataType);
    } else {
        os << "data " << DataTypeToString(pin.type.dataType);
        if (!pin.type.enumType.empty()) {
            os << " enum " << pin.type.enumType;
        }
        if (!pin.type.structType.empty()) {
            os << " struct " << pin.type.structType;
        }
    }
    os << SerializeDefaultValue(pin.defaultValue);
    return os.str();
}

// Strips a trailing '\r' (files saved/edited with CRLF line endings)
// and returns the leading-whitespace-trimmed start index, or
// std::string::npos for a blank line.
std::size_t FirstNonBlank(std::string& line) {
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    return line.find_first_not_of(" \t");
}

} // namespace

// A default value after the word "default": "<kind> <value>" (float, int, bool, vec3, string to the end of the line).
bool ReadDefaultAfterKeyword(std::istringstream& tok, PinDefaultValue& value, std::string& error) {
    std::string kind;
    if (!(tok >> kind)) {
        error = "'default' missing its value kind";
        return false;
    }
    if (kind == "float") {
        float v = 0.0f;
        if (!(tok >> v)) { error = "malformed float value"; return false; }
        value = v;
    } else if (kind == "int") {
        std::int64_t v = 0;
        if (!(tok >> v)) { error = "malformed int value"; return false; }
        value = v;
    } else if (kind == "bool") {
        std::string b;
        if (!(tok >> b) || (b != "true" && b != "false")) { error = "bool value must be 'true' or 'false'"; return false; }
        value = (b == "true");
    } else if (kind == "vec3") {
        Vec3Default v;
        if (!(tok >> v.x >> v.y >> v.z)) { error = "malformed vec3 value"; return false; }
        value = v;
    } else if (kind == "string") {
        std::string rest;
        std::getline(tok, rest);
        if (!rest.empty() && rest.front() == ' ') rest.erase(0, 1);
        value = rest;
    } else {
        error = "unknown value kind '" + kind + "'";
        return false;
    }
    return true;
}

// Struct lines (TYPES.md): "struct <Name>", its display name and description, then per member (in order) its type
// line - "structmember <Name> <dataType> [enum <E>] [struct <S>] [default ...]" - and the lines naming, describing and
// ranging it by index.
void WriteStructLines(std::ostream& os, const std::vector<StructDef>& structs) {
    for (const StructDef& def : structs) {
        os << "struct " << def.name << "\n";
        os << "structname " << def.name << " " << def.displayName << "\n";
        for (size_t i = 0; i < def.members.size(); ++i) {
            const auto& m = def.members[i];
            os << "structmember " << def.name << " " << DataTypeToString(m.type.dataType);
            if (!m.type.enumType.empty()) os << " enum " << m.type.enumType;
            if (!m.type.structType.empty()) os << " struct " << m.type.structType;
            os << SerializeDefaultValue(m.defaultValue) << "\n";
            os << "structmembername " << def.name << " " << i << " " << m.name << "\n";
            if (!m.description.empty()) os << "structmemberdescription " << def.name << " " << i << " " << m.description << "\n";
            if (m.hasRange) os << "structmemberrange " << def.name << " " << i << " " << m.minimum << " " << m.maximum << "\n";
        }
        if (!def.description.empty()) {
            os << "structdescription " << def.name << " " << def.description << "\n";
        }
    }
}

bool ReadStructLine(const std::string& keyword, std::istringstream& tok, std::vector<StructDef>& structs, TypeScope scope, std::string& error) {
    std::string name;
    if (!(tok >> name)) {
        error = "malformed '" + keyword + "' line";
        return false;
    }
    auto find = [&structs](const std::string& n) -> StructDef* {
        for (auto& s : structs)
            if (s.name == n)
                return &s;
        return nullptr;
    };
    if (keyword == "struct") {
        if (find(name) != nullptr) { error = "duplicate struct '" + name + "'"; return false; }
        StructDef def;
        def.name = name;
        def.displayName = name;
        def.scope = scope;
        structs.push_back(std::move(def));
        return true;
    }
    StructDef* def = find(name);
    if (def == nullptr) { error = "'" + keyword + "' for unknown struct '" + name + "'"; return false; }
    auto restOf = [&tok]() {
        std::string rest;
        std::getline(tok, rest);
        if (!rest.empty() && rest.front() == ' ') rest.erase(0, 1);
        return rest;
    };
    if (keyword == "structname") { def->displayName = restOf(); return true; }
    if (keyword == "structdescription") { def->description = restOf(); return true; }
    if (keyword == "structmember") {
        StructMember member;
        std::string dataTypeText;
        if (!(tok >> dataTypeText)) { error = "malformed 'structmember' line"; return false; }
        const auto dataType = DataTypeFromString(dataTypeText);
        if (!dataType) { error = "unknown data type '" + dataTypeText + "'"; return false; }
        member.type = { PinKind::Data, *dataType };
        std::string word;
        while (tok >> word) {
            if (word == "enum") { tok >> member.type.enumType; }
            else if (word == "struct") { tok >> member.type.structType; }
            else if (word == "default") { if (!ReadDefaultAfterKeyword(tok, member.defaultValue, error)) return false; break; }
            else { error = "unexpected '" + word + "' on 'structmember'"; return false; }
        }
        def->members.push_back(std::move(member));
        return true;
    }
    size_t index = 0;
    if (!(tok >> index) || index >= def->members.size()) { error = "'" + keyword + "' for a member '" + name + "' does not have"; return false; }
    auto& member = def->members[index];
    if (keyword == "structmembername") { member.name = restOf(); return true; }
    if (keyword == "structmemberdescription") { member.description = restOf(); return true; }
    if (keyword == "structmemberrange") {
        if (!(tok >> member.minimum >> member.maximum)) { error = "malformed 'structmemberrange'"; return false; }
        member.hasRange = true;
        return true;
    }
    error = "unknown struct keyword '" + keyword + "'";
    return false;
}

// Enum lines (TYPES.md): "enum <Name>", then its display name, values and descriptions.
void WriteEnumLines(std::ostream& os, const std::vector<EnumDef>& enums) {
    for (const EnumDef& def : enums) {
        os << "enum " << def.name << "\n";
        os << "enumname " << def.name << " " << def.displayName << "\n";
        for (size_t i = 0; i < def.variants.size(); ++i) {
            const auto& variant = def.variants[i];
            os << "enumvariant " << def.name << " " << variant.name << "\n";
            if (variant.colour != 0) {
                os << "enumvariantcolour " << def.name << " " << i << " " << std::hex << variant.colour << std::dec << "\n";
            }
            if (!variant.description.empty()) {
                os << "enumvariantdescription " << def.name << " " << i << " " << variant.description << "\n";
            }
        }
        if (!def.description.empty()) {
            os << "enumdescription " << def.name << " " << def.description << "\n";
        }
    }
}

// One enum line into `enums`; false with an error if it is malformed. Handles every "enum..." keyword.
bool ReadEnumLine(const std::string& keyword, std::istringstream& tok, std::vector<EnumDef>& enums, TypeScope scope, std::string& error) {
    auto find = [&enums](const std::string& name) -> EnumDef* {
        for (auto& def : enums)
            if (def.name == name)
                return &def;
        return nullptr;
    };
    std::string name;
    if (!(tok >> name)) {
        error = "malformed '" + keyword + "' line";
        return false;
    }
    if (keyword == "enum") {
        if (find(name) != nullptr || name.find_first_of(" \t") != std::string::npos) {
            error = "duplicate enum '" + name + "'";
            return false;
        }
        EnumDef def;
        def.name = name;
        def.displayName = name;
        def.scope = scope;
        enums.push_back(std::move(def));
        return true;
    }
    EnumDef* def = find(name);
    if (def == nullptr) {
        error = "'" + keyword + "' for unknown enum '" + name + "'";
        return false;
    }
    auto restOf = [&tok]() {
        std::string rest;
        std::getline(tok, rest);
        if (!rest.empty() && rest.front() == ' ') {
            rest.erase(0, 1);
        }
        return rest;
    };
    if (keyword == "enumname") {
        def->displayName = restOf();
    } else if (keyword == "enumvariant") {
        def->variants.push_back(EnumVariant(restOf()));
    } else if (keyword == "enumdescription") {
        def->description = restOf();
    } else if (keyword == "enumvariantcolour" || keyword == "enumvariantdescription") {
        size_t index = 0;
        if (!(tok >> index) || index >= def->variants.size()) {
            error = "'" + keyword + "' for a value enum '" + name + "' does not have";
            return false;
        }
        if (keyword == "enumvariantcolour") {
            std::uint32_t colour = 0;
            if (!(tok >> std::hex >> colour)) {
                error = "malformed colour on '" + keyword + "'";
                return false;
            }
            tok >> std::dec;
            def->variants[index].colour = colour;
        } else {
            def->variants[index].description = restOf();
        }
    } else {
        error = "unknown enum keyword '" + keyword + "'";
        return false;
    }
    return true;
}

std::string SerializeGraph(const Graph& graph) {
    std::ostringstream os;
    os << std::setprecision(9);
    os << "frgraph 1\n";
    os << "graph " << graph.Name() << "\n";
    os << "target " << GraphTargetToString(graph.Target()) << "\n";
    // The graph's type (GRAPH_TYPES.md), only when it has one, so untyped graphs serialise exactly as before.
    if (!graph.DiagramType().empty()) {
        os << "diagram " << graph.DiagramType() << "\n";
    }

    // The graph's own enums (TYPES.md), before the symbols that use them; absent for a graph without any.
    WriteEnumLines(os, graph.Enums());
    WriteStructLines(os, graph.Structs());

    // Graph symbols (symbols.h), in order. Absent entirely for a graph without any, so older graphs serialise
    // byte-for-byte as before.
    for (const Symbol& symbol : graph.Symbols()) {
        os << "symbol " << SymbolKindToString(symbol.kind) << " " << symbol.id << " " << DataTypeToString(symbol.type) << " "
           << symbol.accessibility << " " << (symbol.persistent ? 1 : 0) << SerializeDefaultValue(symbol.value) << "\n";
        os << "symbolname " << symbol.id << " " << symbol.name << "\n";
        if (!symbol.enumType.empty()) {
            os << "symbolenum " << symbol.id << " " << symbol.enumType << "\n";
        }
        if (!symbol.structType.empty()) {
            os << "symbolstruct " << symbol.id << " " << symbol.structType << "\n";
            for (size_t i = 0; i < symbol.memberValues.size(); ++i) {
                os << "symbolmember " << symbol.id << " " << i << SerializeDefaultValue(symbol.memberValues[i]) << "\n";
            }
        }
        if (!symbol.description.empty()) {
            os << "symboldescription " << symbol.id << " " << symbol.description << "\n";
        }
    }

    std::vector<NodeId> nodeIds;
    nodeIds.reserve(graph.Nodes().size());
    for (const auto& [id, node] : graph.Nodes()) {
        nodeIds.push_back(id);
    }
    std::sort(nodeIds.begin(), nodeIds.end());

    for (NodeId id : nodeIds) {
        const Node* node = graph.FindNode(id);
        os << "node " << id << " " << node->TypeName() << " " << DomainToString(node->NodeDomain()) << " "
           << node->EditorX() << " " << node->EditorY() << "\n";
        for (const Pin& pin : node->Inputs()) {
            os << SerializePinLine(id, pin) << "\n";
        }
        for (const Pin& pin : node->Outputs()) {
            os << SerializePinLine(id, pin) << "\n";
        }
        // Per-instance generic type-parameter bindings (real monomorphized
        // generics for Schematic nodes plan, Phase 2) -- own line per
        // binding, same "detail line follows its node line, keyed by
        // nodeId" convention as pin lines above. Absent entirely for a
        // non-generic node (the common case) or an unresolved parameter,
        // same as an ordinary node having zero pin lines of some kind.
        for (const auto& [paramName, dataType] : node->GenericBindings()) {
            os << "genericbinding " << id << " " << paramName << " " << DataTypeToString(dataType) << "\n";
        }
    }

    std::vector<Connection> connections = graph.Connections();
    std::sort(connections.begin(), connections.end(), [](const Connection& a, const Connection& b) { return a.id < b.id; });
    for (const Connection& c : connections) {
        os << "connection " << c.id << " " << c.fromNode << " " << c.fromPin << " " << c.toNode << " " << c.toPin
           << "\n";
    }

    return os.str();
}

std::unique_ptr<Graph> DeserializeGraph(const std::string& text, std::string& errorOut) {
    std::istringstream lines(text);
    std::string line;
    int lineNo = 0;
    bool sawHeader = false;
    std::unique_ptr<Graph> graph;
    std::vector<EnumDef> pendingEnums; // the graph's own enums, added when it is complete
    std::vector<StructDef> pendingStructs; // and structs
    GraphTarget target = GraphTarget::Behavior;

    auto fail = [&](const std::string& msg) -> std::unique_ptr<Graph> {
        errorOut = "line " + std::to_string(lineNo) + ": " + msg;
        return nullptr;
    };

    while (std::getline(lines, line)) {
        ++lineNo;
        const std::size_t contentStart = FirstNonBlank(line);
        if (contentStart == std::string::npos || line[contentStart] == '#') {
            continue; // blank line or comment.
        }

        std::istringstream tok(line);
        std::string keyword;
        tok >> keyword;

        if (keyword == "frgraph") {
            int version = 0;
            if (!(tok >> version) || version != 1) {
                return fail("expected 'frgraph 1' header");
            }
            sawHeader = true;
        } else if (keyword == "graph") {
            if (!sawHeader) {
                return fail("'graph' line before 'frgraph' header");
            }
            std::string name;
            std::getline(tok, name);
            if (!name.empty() && name.front() == ' ') {
                name.erase(0, 1);
            }
            graph = std::make_unique<Graph>(name, target);
        } else if (keyword == "target") {
            if (!sawHeader) {
                return fail("'target' line before 'frgraph' header");
            }
            std::string targetText;
            if (!(tok >> targetText)) {
                return fail("'target' line missing its graph target");
            }
            const auto parsedTarget = GraphTargetFromString(targetText);
            if (!parsedTarget) {
                return fail("unknown graph target '" + targetText + "'");
            }
            target = *parsedTarget;
            if (graph) {
                graph->SetTarget(target);
            }
        } else if (keyword == "diagram") {
            std::string type;
            if (!graph || !(tok >> type)) {
                return fail("malformed 'diagram' line (expected: diagram <type>)");
            }
            graph->SetDiagramType(type);
        } else if (keyword.rfind("struct", 0) == 0) {
            if (!graph) {
                return fail("'" + keyword + "' line before 'graph' line");
            }
            std::string structError;
            if (!ReadStructLine(keyword, tok, pendingStructs, TypeScope::graph, structError)) {
                return fail(structError);
            }
        } else if (keyword == "symbolstruct" || keyword == "symbolmember") {
            std::string id;
            if (!graph || !(tok >> id)) {
                return fail("malformed '" + keyword + "' line");
            }
            Symbol* symbol = graph->FindSymbol(id);
            if (symbol == nullptr) {
                return fail("'" + keyword + "' for unknown symbol '" + id + "'");
            }
            if (keyword == "symbolstruct") {
                tok >> symbol->structType;
            } else {
                size_t index = 0;
                std::string word, valueError;
                PinDefaultValue value;
                if (!(tok >> index)) {
                    return fail("malformed 'symbolmember' line");
                }
                if (tok >> word && word == "default" && !ReadDefaultAfterKeyword(tok, value, valueError)) {
                    return fail(valueError);
                }
                if (symbol->memberValues.size() <= index) {
                    symbol->memberValues.resize(index + 1);
                }
                symbol->memberValues[index] = value;
            }
        } else if (keyword.rfind("enum", 0) == 0) {
            // The graph's own enums are read into a list and added once the graph is complete (see below).
            if (!graph) {
                return fail("'" + keyword + "' line before 'graph' line");
            }
            std::string enumError;
            if (!ReadEnumLine(keyword, tok, pendingEnums, TypeScope::graph, enumError)) {
                return fail(enumError);
            }
        } else if (keyword == "symbol") {
            if (!graph) {
                return fail("'symbol' line before 'graph' line");
            }
            std::string kindText, id, typeText, access;
            int persistent = 0;
            if (!(tok >> kindText >> id >> typeText >> access >> persistent)) {
                return fail("malformed 'symbol' line (expected: symbol <kind> <id> <dataType> <accessibility> <persistent> [default ...])");
            }
            const auto kind = SymbolKindFromString(kindText);
            if (!kind) {
                return fail("unknown symbol kind '" + kindText + "'");
            }
            const auto type = DataTypeFromString(typeText);
            if (!type) {
                return fail("unknown data type '" + typeText + "'");
            }
            PinDefaultValue value;
            std::string defaultKeyword;
            if (tok >> defaultKeyword) {
                if (defaultKeyword != "default") {
                    return fail("unexpected trailing token '" + defaultKeyword + "' on 'symbol' line");
                }
                std::string valueKind;
                tok >> valueKind;
                if (valueKind == "float") {
                    float v = 0.0f;
                    if (!(tok >> v)) return fail("malformed float symbol value");
                    value = v;
                } else if (valueKind == "int") {
                    std::int64_t v = 0;
                    if (!(tok >> v)) return fail("malformed int symbol value");
                    value = v;
                } else if (valueKind == "bool") {
                    std::string b;
                    if (!(tok >> b) || (b != "true" && b != "false")) return fail("symbol bool value must be 'true' or 'false'");
                    value = (b == "true");
                } else if (valueKind == "vec3") {
                    Vec3Default v;
                    if (!(tok >> v.x >> v.y >> v.z)) return fail("malformed vec3 symbol value");
                    value = v;
                } else if (valueKind == "string") {
                    std::string rest;
                    std::getline(tok, rest);
                    if (!rest.empty() && rest.front() == ' ') {
                        rest.erase(0, 1);
                    }
                    value = rest;
                } else {
                    return fail("unknown symbol value kind '" + valueKind + "'");
                }
            }
            Symbol symbol;
            symbol.id = id;
            symbol.name = id;
            symbol.kind = *kind;
            symbol.type = *type;
            symbol.value = value;
            symbol.accessibility = access;
            symbol.persistent = persistent != 0;
            if (!graph->AddSymbol(symbol)) {
                return fail("duplicate symbol id '" + id + "'");
            }
        } else if (keyword == "symbolname" || keyword == "symboldescription") {
            std::string id, rest;
            if (!graph || !(tok >> id)) {
                return fail("malformed '" + keyword + "' line");
            }
            Symbol* symbol = graph->FindSymbol(id);
            if (symbol == nullptr) {
                return fail("'" + keyword + "' for unknown symbol '" + id + "'");
            }
            std::getline(tok, rest);
            if (!rest.empty() && rest.front() == ' ') {
                rest.erase(0, 1);
            }
            (keyword == "symbolname" ? symbol->name : symbol->description) = rest;
        } else if (keyword == "symbolenum") {
            std::string id, enumName;
            if (!graph || !(tok >> id >> enumName)) {
                return fail("malformed 'symbolenum' line (expected: symbolenum <id> <EnumName>)");
            }
            Symbol* symbol = graph->FindSymbol(id);
            if (symbol == nullptr) {
                return fail("'symbolenum' for unknown symbol '" + id + "'");
            }
            symbol->enumType = enumName;
        } else if (keyword == "node") {
            if (!graph) {
                return fail("'node' line before 'graph' line");
            }
            NodeId id = 0;
            std::string typeName, domainStr;
            float x = 0.0f, y = 0.0f;
            if (!(tok >> id >> typeName >> domainStr >> x >> y)) {
                return fail("malformed 'node' line (expected: node <id> <typeName> <domain> <editorX> <editorY>)");
            }
            const auto domain = DomainFromString(domainStr);
            if (!domain) {
                return fail("unknown domain '" + domainStr + "'");
            }
            Node* node = graph->AddNodeWithId(id, typeName, *domain);
            if (node == nullptr) {
                return fail("duplicate node id " + std::to_string(id));
            }
            node->SetEditorPosition(x, y);
        } else if (keyword == "pin") {
            if (!graph) {
                return fail("'pin' line before 'graph' line");
            }
            NodeId nodeId = 0;
            std::string direction;
            PinId pinId = 0;
            std::string name, kindStr;
            if (!(tok >> nodeId >> direction >> pinId >> name >> kindStr)) {
                return fail("malformed 'pin' line (expected: pin <nodeId> in|out <pinId> <name> exec|data ...)");
            }
            Node* node = graph->FindNode(nodeId);
            if (node == nullptr) {
                return fail("'pin' references unknown node " + std::to_string(nodeId));
            }
            if (node->FindPin(pinId) != nullptr) {
                return fail("duplicate pin id " + std::to_string(pinId) + " on node " + std::to_string(nodeId));
            }

            bool isInput;
            if (direction == "in") {
                isInput = true;
            } else if (direction == "out") {
                isInput = false;
            } else {
                return fail("unknown pin direction '" + direction + "' (expected 'in' or 'out')");
            }

            PinTypeDesc type;
            if (kindStr == "exec") {
                type.kind = PinKind::Exec;
            } else if (kindStr == "data" || kindStr == "stream") {
                type.kind = kindStr == "data" ? PinKind::Data : PinKind::Stream;
                std::string dataTypeStr;
                if (!(tok >> dataTypeStr)) {
                    return fail("'pin ... " + kindStr + "' line missing its data type");
                }
                const auto dataType = DataTypeFromString(dataTypeStr);
                if (!dataType) {
                    return fail("unknown data type '" + dataTypeStr + "'");
                }
                type.dataType = *dataType;
            } else {
                return fail("unknown pin kind '" + kindStr + "' (expected 'exec', 'data', or 'stream')");
            }

            PinDefaultValue defaultValue;
            std::string maybeDefaultKeyword;
            bool haveKeyword = static_cast<bool>(tok >> maybeDefaultKeyword);
            if (haveKeyword && maybeDefaultKeyword == "enum") {
                if (type.kind != PinKind::Data || !(tok >> type.enumType)) {
                    return fail("malformed 'enum' on 'pin' line (expected: data <dataType> enum <EnumName>)");
                }
                haveKeyword = static_cast<bool>(tok >> maybeDefaultKeyword);
            }
            if (haveKeyword && maybeDefaultKeyword == "struct") {
                if (type.kind != PinKind::Data || !(tok >> type.structType)) {
                    return fail("malformed 'struct' on 'pin' line (expected: data struct struct <StructName>)");
                }
                haveKeyword = static_cast<bool>(tok >> maybeDefaultKeyword);
            }
            if (haveKeyword) {
                if (maybeDefaultKeyword != "default") {
                    return fail("unexpected trailing token '" + maybeDefaultKeyword + "' on 'pin' line");
                }
                std::string valueKind;
                if (!(tok >> valueKind)) {
                    return fail("'default' missing its value kind");
                }
                if (valueKind == "float") {
                    float v = 0.0f;
                    if (!(tok >> v)) return fail("malformed float default value");
                    defaultValue = v;
                } else if (valueKind == "int") {
                    std::int64_t v = 0;
                    if (!(tok >> v)) return fail("malformed int default value");
                    defaultValue = v;
                } else if (valueKind == "bool") {
                    std::string b;
                    if (!(tok >> b)) return fail("malformed bool default value");
                    if (b != "true" && b != "false") return fail("bool default must be 'true' or 'false'");
                    defaultValue = (b == "true");
                } else if (valueKind == "vec3") {
                    Vec3Default v;
                    if (!(tok >> v.x >> v.y >> v.z)) return fail("malformed vec3 default value");
                    defaultValue = v;
                } else if (valueKind == "string") {
                    std::string rest;
                    std::getline(tok, rest);
                    if (!rest.empty() && rest.front() == ' ') {
                        rest.erase(0, 1);
                    }
                    defaultValue = rest;
                } else {
                    return fail("unknown default value kind '" + valueKind + "'");
                }
            }

            if (isInput) {
                node->AddInputWithId(pinId, name, type, defaultValue);
            } else {
                node->AddOutputWithId(pinId, name, type, defaultValue);
            }
        } else if (keyword == "genericbinding") {
            if (!graph) {
                return fail("'genericbinding' line before 'graph' line");
            }
            NodeId nodeId = 0;
            std::string paramName, dataTypeStr;
            if (!(tok >> nodeId >> paramName >> dataTypeStr)) {
                return fail("malformed 'genericbinding' line (expected: genericbinding <nodeId> <paramName> <dataType>)");
            }
            Node* node = graph->FindNode(nodeId);
            if (node == nullptr) {
                return fail("'genericbinding' references unknown node " + std::to_string(nodeId));
            }
            const auto dataType = DataTypeFromString(dataTypeStr);
            if (!dataType) {
                return fail("unknown data type '" + dataTypeStr + "' on 'genericbinding' line");
            }
            node->SetGenericBinding(paramName, *dataType);
        } else if (keyword == "connection") {
            if (!graph) {
                return fail("'connection' line before 'graph' line");
            }
            ConnectionId id = 0;
            NodeId fromNode = 0, toNode = 0;
            PinId fromPin = 0, toPin = 0;
            if (!(tok >> id >> fromNode >> fromPin >> toNode >> toPin)) {
                return fail("malformed 'connection' line (expected: connection <id> <fromNode> <fromPin> <toNode> <toPin>)");
            }
            ConnectError connectError = ConnectError::UnknownNode;
            const auto result = graph->ConnectWithId(id, fromNode, fromPin, toNode, toPin, &connectError);
            if (!result) {
                return fail("connection " + std::to_string(id) + " is invalid (duplicate id, unknown node/pin, "
                             "wrong pin direction, or incompatible types)");
            }
        } else {
            return fail("unknown line keyword '" + keyword + "'");
        }
    }

    if (!sawHeader) {
        return fail("missing 'frgraph 1' header");
    }
    if (!graph) {
        return fail("missing 'graph' line");
    }
    for (auto& def : pendingEnums) {
        if (!graph->AddEnum(std::move(def))) {
            return fail("duplicate enum");
        }
    }
    for (auto& def : pendingStructs) {
        if (!graph->AddStruct(std::move(def))) {
            return fail("duplicate struct");
        }
    }
    return graph;
}

std::string SerializeTypes(const std::vector<EnumDef>& enums, const std::vector<StructDef>& structs) {
    std::ostringstream os;
    os << std::setprecision(9);
    os << "frtypes 1\n";
    WriteEnumLines(os, enums);
    WriteStructLines(os, structs);
    return os.str();
}

bool DeserializeTypes(const std::string& text, TypeScope scope, std::vector<EnumDef>& enums, std::string& error) {
    std::vector<StructDef> structs;
    return DeserializeTypes(text, scope, enums, structs, error);
}

bool DeserializeTypes(const std::string& text, TypeScope scope, std::vector<EnumDef>& enums, std::vector<StructDef>& structs, std::string& error) {
    enums.clear();
    structs.clear();
    std::istringstream in(text);
    std::string line;
    bool header = false;
    int lineNumber = 0;
    while (std::getline(in, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.find_first_not_of(" \t") == std::string::npos || line.rfind("#", 0) == 0) {
            continue;
        }
        std::istringstream tok(line);
        std::string keyword;
        tok >> keyword;
        if (!header) {
            if (keyword != "frtypes") {
                error = "not a types file (expected 'frtypes 1')";
                return false;
            }
            header = true;
            continue;
        }
        const bool ok = keyword.rfind("struct", 0) == 0 ? ReadStructLine(keyword, tok, structs, scope, error)
                      : keyword.rfind("enum", 0) == 0 ? ReadEnumLine(keyword, tok, enums, scope, error) : false;
        if (!ok) {
            if (error.empty()) {
                error = "unknown line '" + keyword + "'";
            }
            error = "line " + std::to_string(lineNumber) + ": " + error;
            return false;
        }
    }
    return true;
}

} // namespace ce::node_system
