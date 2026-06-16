#include "gfx/materials.h"

// IWYU pragma: begin_keep
#include <Corrade/Containers/OptionalStl.h>
#include <Corrade/Containers/PairStl.h>
#include <Corrade/Containers/StringStl.h>
// IWYU pragma: end_keep

#include <Corrade/Containers/Array.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Magnum/GL/Context.h>
#include <Magnum/GL/Extensions.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/MaterialData.h>
#include <Magnum/Trade/PbrMetallicRoughnessMaterialData.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/Trade/TextureData.h>
#include <optional>
#include "gfx/imageUtil.h"

using namespace Magnum;

namespace kitgui {
void MaterialCache::LoadTextures(Magnum::Trade::AbstractImporter& importer) {
    /* Load all textures. Textures that fail to load will be NullOpt. */
    // Debug{} << "Loading" << importer.textureCount() << "textures";
    mTextures.clear();
    mTextures.reserve(importer.textureCount());

    for (uint32_t i = 0; i != importer.textureCount(); ++i) {
        auto textureData = std::optional<Trade::TextureData>{importer.texture(i)};
        if (!textureData || textureData->type() != Trade::TextureType::Texture2D) {
            // kitgui::log::error(fmt::format("cannot load texture {}, {} ", i, importer.textureName(i)));
            continue;
        }

        auto imageData = std::optional<Trade::ImageData2D>{importer.image2D(textureData->image())};
        if (!imageData) {
            /*
            kitgui::log::error(fmt::format("cannot load image {}, {} ", textureData->image(),
                                           importer.image2DName(textureData->image())));
            */
            continue;
        }

        /* Configure the texture */
        GL::Texture2D texture;
        texture.setMagnificationFilter(textureData->magnificationFilter())
            .setMinificationFilter(textureData->minificationFilter(), textureData->mipmapFilter())
            .setWrapping(textureData->wrapping().xy());

        LoadImageToTexture(*imageData, texture);
        mTextures.push_back({std::move(texture)});
    }
}

void MaterialCache::LoadMaterials(Magnum::Trade::AbstractImporter& importer) {
    /* Load all materials. Materials that fail to load will be NullOpt. The
       data will be stored directly in objects later, so save them only
       temporarily. */
    // kitgui::log::verbose(fmt::format("Loading {} materials", importer.materialCount()));
    mMaterials.clear();
    mMaterials.reserve(importer.materialCount());
    for (uint32_t i = 0; i != importer.materialCount(); ++i) {
        mMaterials.push_back({});
        MaterialInfo& mat = mMaterials.back();
        mat.raw = std::optional<Trade::MaterialData>{importer.material(i)};
        const auto& materialData = mat.raw;

        if (!materialData || !(materialData->types() & Trade::MaterialType::Phong) ||
            (materialData->as<Trade::PhongMaterialData>().hasTextureTransformation() &&
             !materialData->as<Trade::PhongMaterialData>().hasCommonTextureTransformation()) ||
            materialData->as<Trade::PhongMaterialData>().hasTextureCoordinates()) {
            // kitgui::log::error(fmt::format("Cannont load material {} {}", i, importer.materialName(i)));
            mat.raw = std::nullopt;
            continue;
        }
    }
}

const Magnum::Trade::PhongMaterialData* MaterialInfo::Phong() const {
    return raw.has_value() && raw->types() & Magnum::Trade::MaterialType::Phong
               ? &(raw->as<Magnum::Trade::PhongMaterialData>())
               : nullptr;
}

const Magnum::Trade::PbrMetallicRoughnessMaterialData* MaterialInfo::Pbr() const {
    return raw.has_value() && raw->types() & Magnum::Trade::MaterialType::PbrMetallicRoughness
               ? &(raw->as<Magnum::Trade::PbrMetallicRoughnessMaterialData>())
               : nullptr;
}

void MaterialCache::ConfigureBasisLoader(Corrade::PluginManager::PluginMetadata& importer) {
    GL::Context& context = GL::Context::current();
    using namespace GL::Extensions;

    // importer.configuration().setValue("assumeYUp", true);

    // This is pared down from the source by assuming we only want to support desktop opengl

    // LDR format
    if (context.isExtensionSupported<KHR::texture_compression_astc_ldr>()) {
        importer.configuration().setValue("format", "Astc4x4RGBA");
    } else if (context.isExtensionSupported<ARB::texture_compression_bptc>()) {
        importer.configuration().setValue("format", "Bc7RGBA");
    } else if (context.isExtensionSupported<EXT::texture_compression_s3tc>()) {
        importer.configuration().setValue("format", "Bc3RGBA");
    } else {
        /* Fall back to uncompressed if nothing else is supported */
        importer.configuration().setValue("format", "RGBA8");
    }

    // HDR format
    if (context.isExtensionSupported<KHR::texture_compression_astc_hdr>()) {
        importer.configuration().setValue("formatHdr", "Astc4x4RGBAF");
    } else if (context.isExtensionSupported<ARB::texture_compression_bptc>()) {
        importer.configuration().setValue("formatHdr", "Bc6hRGB");
    } else {
        /* Fall back to uncompressed if nothing else is supported */
        importer.configuration().setValue("formatHdr", "RGBA16F");
    }
}
}  // namespace kitgui