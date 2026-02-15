#pragma once

#include <Corrade/PluginManager/PluginMetadata.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/Trade/MaterialData.h>
#include <optional>
#include <vector>

namespace Magnum {
namespace Trade {
class AbstractImporter;
class PhongMaterialData;
}  // namespace Trade
}  // namespace Magnum

namespace kitgui {

struct TextureInfo {
    std::optional<Magnum::GL::Texture2D> texture;
};

struct MaterialInfo {
    std::optional<Magnum::Trade::MaterialData> raw;
    const Magnum::Trade::PhongMaterialData* Phong() const;
};

struct MaterialCache {
    std::vector<MaterialInfo> mMaterials;
    std::vector<TextureInfo> mTextures;

    void LoadTextures(Magnum::Trade::AbstractImporter& importer);
    void LoadMaterials(Magnum::Trade::AbstractImporter& importer);

    void ConfigureBasisLoader(Corrade::PluginManager::PluginMetadata& importer);
};
}  // namespace kitgui