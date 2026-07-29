#pragma once

#include "creation/assets/AssetCatalog.h"

namespace creation::assets
{
class AssetResolver final
{
public:
    static const AssetDescriptor* resolve(const AssetCatalog& catalog, const AssetRef& reference) noexcept;
};
}
