#include <creation/video/NativeVideoNodeRegistry.h>
#include <creation/video/ColorBalanceNode.h>
#include <creation/video/GaussianBlurNode.h>
#include <creation/video/TransformCropNode.h>

// Needed for a reason that isn't obvious from this file's own code: constructing a node here via
// std::make_unique<ColorBalanceNode>(...) and returning it as unique_ptr<INativeVideoNode>
// forces each concrete node's destructor to be instantiated at this point (the derived-to-base
// unique_ptr conversion needs it), which in turn instantiates ~ComPtr<ID3D11Buffer>() etc. -
// those need the real D3D11 interface definitions, which the node headers only forward-declare.
#include <d3d11.h>

namespace creation::video
{
namespace
{
using ce::node_system::DataType;
using ce::node_system::Domain;
using ce::node_system::NodeTypeDescriptor;
using ce::node_system::NodeTypeRegistry;
using ce::node_system::PinDefaultValue;
using ce::node_system::PinKind;
using ce::node_system::PinSignature;
using ce::node_system::PinTypeDesc;

// No default: an unconnected texture input isn't a meaningful state for a video node - it always
// needs a real frame wired in.
PinSignature texturePin(std::string name)
{
    return { std::move(name), PinTypeDesc { PinKind::Data, DataType::Texture }, PinDefaultValue {} };
}

PinSignature floatParamPin(std::string name, float defaultValue)
{
    return { std::move(name), PinTypeDesc { PinKind::Data, DataType::Float }, defaultValue };
}
}

void RegisterNativeVideoNodeTypes(NodeTypeRegistry& registry)
{
    registry.Register(NodeTypeDescriptor {
        "ColorBalance", Domain::Video,
        {
            texturePin("input"),
            floatParamPin("exposure", 0.0f),
            floatParamPin("contrast", 1.0f),
            floatParamPin("saturation", 1.0f),
            floatParamPin("tintR", 1.0f),
            floatParamPin("tintG", 1.0f),
            floatParamPin("tintB", 1.0f),
        },
        {
            texturePin("output"),
        }
    });

    registry.Register(NodeTypeDescriptor {
        "GaussianBlur", Domain::Video,
        {
            texturePin("input"),
            floatParamPin("radius", 4.0f),
        },
        {
            texturePin("output"),
        }
    });

    registry.Register(NodeTypeDescriptor {
        "TransformCrop", Domain::Video,
        {
            texturePin("input"),
            floatParamPin("scaleX", 1.0f),
            floatParamPin("scaleY", 1.0f),
            floatParamPin("translateX", 0.0f),
            floatParamPin("translateY", 0.0f),
            floatParamPin("rotationDegrees", 0.0f),
            floatParamPin("cropLeft", 0.0f),
            floatParamPin("cropTop", 0.0f),
            floatParamPin("cropRight", 0.0f),
            floatParamPin("cropBottom", 0.0f),
        },
        {
            texturePin("output"),
        }
    });
}

std::unique_ptr<INativeVideoNode> CreateNativeVideoNode(const std::string& typeName, ID3D11Device* device)
{
    if (typeName == "ColorBalance")
        return std::make_unique<ColorBalanceNode>(device);
    if (typeName == "GaussianBlur")
        return std::make_unique<GaussianBlurNode>(device);
    if (typeName == "TransformCrop")
        return std::make_unique<TransformCropNode>(device);
    return nullptr;
}
}
