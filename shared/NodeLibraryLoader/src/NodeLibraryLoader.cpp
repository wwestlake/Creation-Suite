#include "creation/frust/NodeLibraryLoader.h"

#include <optional>
#include <set>

#include <juce_core/juce_core.h>

namespace creation::frust {
using namespace ce::node_system;
namespace {

std::optional<Domain> domain(const juce::String& value) {
    if (value == "core") return Domain::Core;
    if (value == "event") return Domain::Event;
    if (value == "animation") return Domain::Animation;
    if (value == "material") return Domain::Material;
    if (value == "audio") return Domain::Audio;
    if (value == "video") return Domain::Video;
    if (value == "input") return Domain::Input;
    return std::nullopt;
}
std::optional<GraphTarget> target(const juce::String& value) {
    if (value == "behavior") return GraphTarget::Behavior;
    if (value == "material") return GraphTarget::Material;
    if (value == "dataflow") return GraphTarget::Dataflow;
    return std::nullopt;
}

std::optional<DataType> dataType(const juce::String& value) {
    // "any" is FRust's honest fallback for a pin type it has no precise
    // mapping for yet (a generic node parameter, a struct, Vector<T>, ...).
    // DataType::Any already exists and already wildcard-matches in
    // IsConnectionCompatible.
    if (value == "any") return DataType::Any;
    if (value == "float") return DataType::Float;
    if (value == "vec2") return DataType::Vec2;
    if (value == "vec3") return DataType::Vec3;
    if (value == "vec4") return DataType::Vec4;
    if (value == "color") return DataType::Color;
    if (value == "bool") return DataType::Bool;
    if (value == "int") return DataType::Int;
    if (value == "string") return DataType::String;
    if (value == "transform") return DataType::Transform;
    if (value == "boneTransform") return DataType::BoneTransform;
    if (value == "texture") return DataType::Texture;
    if (value == "audioSignal") return DataType::AudioSignal;
    if (value == "entity") return DataType::Entity;
    if (value == "function") return DataType::Function;
    return std::nullopt;
}

std::optional<PinKind> pinKind(const juce::String& value) {
    if (value == "data") return PinKind::Data;
    if (value == "exec") return PinKind::Exec;
    if (value == "stream") return PinKind::Stream;
    return std::nullopt;
}

bool parseDefaultValue(const juce::var& value, PinDefaultValue& destination, std::string& error) {
    if (value.isVoid() || value.isUndefined()) {
        destination = std::monostate{};
        return true;
    }
    if (value.isBool()) {
        destination = static_cast<bool>(value);
        return true;
    }
    if (value.isInt() || value.isInt64()) {
        destination = static_cast<std::int64_t>(value);
        return true;
    }
    if (value.isDouble()) {
        destination = static_cast<float>(static_cast<double>(value));
        return true;
    }
    if (value.isString()) {
        destination = value.toString().toStdString();
        return true;
    }
    if (value.isArray()) {
        const auto* values = value.getArray();
        if (values != nullptr && values->size() == 3 && (*values)[0].isDouble() && (*values)[1].isDouble() && (*values)[2].isDouble()) {
            destination = Vec3Default { static_cast<float>(static_cast<double>((*values)[0])),
                                        static_cast<float>(static_cast<double>((*values)[1])),
                                        static_cast<float>(static_cast<double>((*values)[2])) };
            return true;
        }
    }
    error = "must be null, a scalar, or a three-number vec3 array";
    return false;
}

bool parsePin(const juce::var& value, PinSignature& pin, std::string& error) {
    if (!value.isObject()) {
        error = "must be an object";
        return false;
    }
    const auto* object = value.getDynamicObject();
    pin.name = object->getProperty("name").toString().toStdString();
    if (pin.name.empty()) {
        error = "needs a non-empty name";
        return false;
    }
    const auto kind = pinKind(object->getProperty("kind").toString());
    if (!kind) {
        error = "has an unknown pin kind";
        return false;
    }
    pin.type.kind = *kind;
    if (*kind != PinKind::Exec) {
        const auto type = dataType(object->getProperty("dataType").toString());
        if (!type) {
            error = "has an unknown data type";
            return false;
        }
        pin.type.dataType = *type;
    }
    if (object->hasProperty("default") && !parseDefaultValue(object->getProperty("default"), pin.defaultValue, error)) {
        error = "default " + error;
        return false;
    }
    // Real monomorphized generics for Schematic nodes: present only when
    // this pin's type is one of its node's own genericParams (Codegen.h's
    // compileNodeReflection). Optional, same as "default" above.
    if (object->hasProperty("genericParam")) {
        pin.genericParam = object->getProperty("genericParam").toString().toStdString();
    }
    return true;
}

bool parsePins(const juce::var& value, std::vector<PinSignature>& pins, const char* direction, std::string& error) {
    if (value.isVoid() || value.isUndefined()) {
        return true;
    }
    if (!value.isArray()) {
        error = std::string(direction) + " pins must be an array";
        return false;
    }
    const auto* items = value.getArray();
    for (int index = 0; index < items->size(); ++index) {
        PinSignature pin;
        std::string pinError;
        if (!parsePin((*items)[index], pin, pinError)) {
            error = std::string(direction) + " pin " + std::to_string(index) + " " + pinError;
            return false;
        }
        pins.push_back(std::move(pin));
    }
    return true;
}

bool parseLibrary(const PluginRuntime::NodeLibraryManifest& manifest,
                  NodeLibraryDescriptor& library, std::string& error) {
    juce::var value;
    const auto result = juce::JSON::parse(manifest.descriptorJson, value);
    if (result.failed() || !value.isObject()) {
        error = "Node library '" + manifest.id + "' has invalid JSON.";
        return false;
    }
    const auto* object = value.getDynamicObject();
    library.id = object->getProperty("id").toString().toStdString();
    if (library.id != manifest.id) {
        error = "Node library manifest id does not match its descriptor id.";
        return false;
    }
    const auto graphTarget = target(object->getProperty("target").toString());
    if (!graphTarget) {
        error = "Node library '" + manifest.id + "' has an unknown graph target.";
        return false;
    }
    const auto displayName = object->getProperty("displayName");
    library.displayName = (displayName.isVoid() || displayName.isUndefined())
        ? manifest.id
        : displayName.toString().toStdString();
    library.description = object->getProperty("description").toString().toStdString();
    library.target = *graphTarget;
    const auto sourceModules = object->getProperty("sourceModules");
    if (!sourceModules.isVoid() && !sourceModules.isUndefined()) {
        if (!sourceModules.isArray()) {
            error = "Node library '" + manifest.id + "' has a non-array sourceModules field.";
            return false;
        }
        for (const auto& module : *sourceModules.getArray()) {
            if (!module.isString() || module.toString().isEmpty()) {
                error = "Node library '" + manifest.id + "' has an invalid source module.";
                return false;
            }
            library.frustSourceModules.push_back(module.toString().toStdString());
        }
    }

    const auto nodes = object->getProperty("nodes");
    if (!nodes.isArray()) {
        error = "Node library '" + manifest.id + "' needs a nodes array.";
        return false;
    }
    const auto* nodeArray = nodes.getArray();
    for (int index = 0; index < nodeArray->size(); ++index) {
        const auto& nodeValue = (*nodeArray)[index];
        if (!nodeValue.isObject()) {
            error = "Node library '" + manifest.id + "' node " + std::to_string(index) + " must be an object.";
            return false;
        }
        const auto* nodeObject = nodeValue.getDynamicObject();
        NodeTypeDescriptor node;
        node.typeName = nodeObject->getProperty("typeName").toString().toStdString();
        const auto nodeDisplayName = nodeObject->getProperty("displayName");
        node.displayName = (nodeDisplayName.isVoid() || nodeDisplayName.isUndefined())
            ? node.typeName
            : nodeDisplayName.toString().toStdString();
        node.category = nodeObject->getProperty("category").toString().toStdString();
        node.description = nodeObject->getProperty("description").toString().toStdString();
        node.frustEntryPoint = nodeObject->getProperty("frustEntryPoint").toString().toStdString();
        const auto nodeDomain = domain(nodeObject->getProperty("domain").toString());
        if (node.typeName.empty() || node.frustEntryPoint.empty() || !nodeDomain) {
            error = "Node library '" + manifest.id + "' node " + std::to_string(index) + " needs typeName, frustEntryPoint, and a known domain.";
            return false;
        }
        node.domain = *nodeDomain;
        const auto capabilities = nodeObject->getProperty("requiredCapabilities");
        if (!capabilities.isVoid() && !capabilities.isUndefined()) {
            if (!capabilities.isArray()) {
                error = "Node library '" + manifest.id + "' node " + std::to_string(index) + " has a non-array requiredCapabilities field.";
                return false;
            }
            for (const auto& capability : *capabilities.getArray()) {
                if (!capability.isString()) {
                    error = "Node library '" + manifest.id + "' node " + std::to_string(index) + " has a non-string required capability.";
                    return false;
                }
                node.requiredCapabilities.push_back(capability.toString().toStdString());
            }
        }
        std::string pinsError;
        if (!parsePins(nodeObject->getProperty("inputs"), node.inputs, "input", pinsError) ||
            !parsePins(nodeObject->getProperty("outputs"), node.outputs, "output", pinsError)) {
            error = "Node library '" + manifest.id + "' node " + std::to_string(index) + " " + pinsError + ".";
            return false;
        }
        // Real monomorphized generics for Schematic nodes: straight from
        // FRust reflection's own "genericParams" array (Codegen.h's
        // compileNodeReflection). Optional, same as requiredCapabilities
        // above -- empty/absent for an ordinary node.
        const auto genericParams = nodeObject->getProperty("genericParams");
        if (!genericParams.isVoid() && !genericParams.isUndefined()) {
            if (!genericParams.isArray()) {
                error = "Node library '" + manifest.id + "' node " + std::to_string(index) + " has a non-array genericParams field.";
                return false;
            }
            for (const auto& param : *genericParams.getArray()) {
                if (!param.isString()) {
                    error = "Node library '" + manifest.id + "' node " + std::to_string(index) + " has a non-string generic parameter.";
                    return false;
                }
                node.genericParams.push_back(param.toString().toStdString());
            }
        }
        library.nodeTypes.push_back(std::move(node));
    }
    return true;
}
} // namespace

bool RegisterPluginNodeLibraries(const std::vector<PluginRuntime::NodeLibraryManifest>& manifests,
                                 ce::node_system::NodeLibraryRegistry& registry,
                                 const std::set<std::string, std::less<>>& supportedCapabilities,
                                 std::string& error) {
    std::vector<NodeLibraryDescriptor> libraries;
    libraries.reserve(manifests.size());
    for (const auto& manifest : manifests) {
        NodeLibraryDescriptor library;
        if (!parseLibrary(manifest, library, error)) {
            return false;
        }
        // The actual capability enforcement: requiredCapabilities was
        // already parsed off every node above, but nothing ever checked
        // it against anything -- a library could declare it needed a
        // capability the host doesn't provide and load anyway. Reject
        // the WHOLE library (not just the offending node) with a clear,
        // specific error, so a half-working library never silently loads.
        for (const auto& node : library.nodeTypes) {
            for (const auto& capability : node.requiredCapabilities) {
                if (!supportedCapabilities.contains(capability)) {
                    error = "Node library '" + library.id + "' node '" + node.typeName +
                            "' requires capability '" + capability + "', which this host does not provide.";
                    return false;
                }
            }
        }
        libraries.push_back(std::move(library));
    }

    std::set<std::string, std::less<>> libraryIds;
    std::set<std::string, std::less<>> nodeTypeNames;
    for (const auto& library : libraries) {
        if (registry.FindLibrary(library.id) != nullptr || !libraryIds.insert(library.id).second) {
            error = "Node library '" + library.id + "' is already registered.";
            return false;
        }
        for (const auto& node : library.nodeTypes) {
            if (registry.FindNodeType(node.typeName) != nullptr || !nodeTypeNames.insert(node.typeName).second) {
                error = "Node type '" + node.typeName + "' is already registered.";
                return false;
            }
        }
    }
    for (auto& library : libraries) {
        if (!registry.Register(std::move(library), &error)) {
            return false;
        }
    }
    return true;
}
} // namespace creation::frust
