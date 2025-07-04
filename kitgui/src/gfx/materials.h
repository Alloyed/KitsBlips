#pragma once

#include <Magnum/GL/Texture.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Vector.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/LightData.h>
#include <Magnum/Trade/MaterialData.h>

#include <optional>
#include <string>
#include <vector>

#include "gfx/drawables.h"

namespace kitgui {

struct TextureInfo {
    std::optional<Magnum::GL::Texture2D> texture;
};

struct MaterialInfo {
    std::optional<Magnum::Trade::MaterialData> raw;
    const Magnum::Trade::PhongMaterialData* Phong() const {
        return raw.has_value() && raw->types() & Magnum::Trade::MaterialType::Phong
                   ? &(raw->as<Magnum::Trade::PhongMaterialData>())
                   : nullptr;
    }
};

struct MaterialCache {
    std::vector<MaterialInfo> mMaterials;
    std::vector<TextureInfo> mTextures;

    void LoadTextures(Magnum::Trade::AbstractImporter& importer);
    void LoadMaterials(Magnum::Trade::AbstractImporter& importer);
};
}  // namespace kitgui