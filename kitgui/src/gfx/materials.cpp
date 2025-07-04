#include "gfx/materials.h"

#include <Corrade/Containers/OptionalStl.h>
#include <Corrade/Containers/PairStl.h>
#include <Corrade/Containers/StringStl.h>

#include <Corrade/Containers/Array.h>
#include <Corrade/Utility/Algorithms.h>
#include <Magnum/GL/Context.h>
#include <Magnum/GL/Extensions.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/ImageView.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/MaterialData.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/Trade/TextureData.h>
#include <optional>

using namespace Magnum;

namespace {

void loadImage(Magnum::GL::Texture2D& texture, const Magnum::Trade::ImageData2D& image) {
    if (!image.isCompressed()) {
        Containers::Array<char> usedImageStorage;
        ImageView2D usedImage = image;
        const uint32_t channelCount = pixelFormatChannelCount(image.format());
        if (channelCount == 1 || channelCount == 2) {
            if (GL::Context::current().isExtensionSupported<GL::Extensions::ARB::texture_swizzle>()) {
                if (channelCount == 1)
                    texture.setSwizzle<'r', 'r', 'r', '1'>();
                else if (channelCount == 2)
                    texture.setSwizzle<'r', 'r', 'r', 'g'>();
            } else {
                const PixelFormat imageFormat =
                    pixelFormat(image.format(), channelCount == 2 ? 4 : 3, isPixelFormatSrgb(image.format()));
                Debug{} << "Texture swizzle not supported, expanding a" << image.format() << "image to" << imageFormat;
                const std::size_t rowStride = 4 * ((pixelFormatSize(imageFormat) * image.size().x() + 3) / 4);
                usedImageStorage = Containers::Array<char>{NoInit, std::size_t(rowStride * image.size().y())};
                const MutableImageView2D usedMutableImage{imageFormat, image.size(), usedImageStorage};
                usedImage = usedMutableImage;
                const std::size_t channelSize = pixelFormatSize(pixelFormatChannelFormat(imageFormat));
                const std::size_t dstChannelCount = pixelFormatChannelCount(imageFormat);
                const Containers::StridedArrayView4D<const char> src =
                    image.pixels().expanded<2>(Containers::Size2D{channelCount, channelSize});
                const Containers::StridedArrayView4D<char> dst =
                    usedMutableImage.pixels().expanded<2>(Containers::Size2D{dstChannelCount, channelSize});
                Utility::copy(src.exceptSuffix({0, 0, channelCount == 2 ? 1 : 0, 0}).broadcasted<2>(3),
                              dst.exceptSuffix({0, 0, channelCount == 2 ? 1 : 0, 0}));
                if (channelCount == 2)
                    Utility::copy(src.exceptPrefix({0, 0, 1, 0}), dst.exceptPrefix({0, 0, 3, 0}));
            }
        }
        GL::TextureFormat format;
        switch (usedImage.format()) {
            case PixelFormat::R8Unorm:
            case PixelFormat::RG8Unorm:
            case PixelFormat::RGB8Unorm:
            case PixelFormat::RGB8Srgb:
            case PixelFormat::RGBA8Unorm:
            case PixelFormat::RGBA8Srgb:
            case PixelFormat::R16Unorm:
            case PixelFormat::RG16Unorm:
            case PixelFormat::RGB16Unorm:
            case PixelFormat::RGBA16Unorm:
            case PixelFormat::R16F:
            case PixelFormat::RG16F:
            case PixelFormat::RGB16F:
            case PixelFormat::RGBA16F:
            case PixelFormat::R32F:
            case PixelFormat::RG32F:
            case PixelFormat::RGB32F:
            case PixelFormat::RGBA32F:
                format = GL::textureFormat(usedImage.format());
                break;
            default:
                Warning{} << "Cannot load an image of format" << usedImage.format();
                return;
        }
        texture.setStorage(Math::log2(usedImage.size().max()) + 1, format, usedImage.size())
            .setSubImage(0, {}, usedImage)
            .generateMipmap();
    } else {
        GL::TextureFormat format;
        switch (image.compressedFormat()) {
            case CompressedPixelFormat::Bc4RSnorm:
            case CompressedPixelFormat::Bc5RGSnorm:
            case CompressedPixelFormat::EacR11Snorm:
            case CompressedPixelFormat::EacRG11Snorm:
            case CompressedPixelFormat::Bc6hRGBUfloat:
            case CompressedPixelFormat::Bc6hRGBSfloat:
            case CompressedPixelFormat::Astc4x4RGBAF:
            case CompressedPixelFormat::Astc5x4RGBAF:
            case CompressedPixelFormat::Astc5x5RGBAF:
            case CompressedPixelFormat::Astc6x5RGBAF:
            case CompressedPixelFormat::Astc6x6RGBAF:
            case CompressedPixelFormat::Astc8x5RGBAF:
            case CompressedPixelFormat::Astc8x6RGBAF:
            case CompressedPixelFormat::Astc8x8RGBAF:
            case CompressedPixelFormat::Astc10x5RGBAF:
            case CompressedPixelFormat::Astc10x6RGBAF:
            case CompressedPixelFormat::Astc10x8RGBAF:
            case CompressedPixelFormat::Astc10x10RGBAF:
            case CompressedPixelFormat::Astc12x10RGBAF:
            case CompressedPixelFormat::Astc12x12RGBAF:
                Warning{} << "Cannot load an image of format" << image.compressedFormat();
                return;
            default:
                format = GL::textureFormat(image.compressedFormat());
        }
        texture.setStorage(1, format, image.size()).setCompressedSubImage(0, {}, image);
    }
}
}  // namespace

namespace kitgui {
void MaterialCache::LoadTextures(Magnum::Trade::AbstractImporter& importer) {
    /* Load all textures. Textures that fail to load will be NullOpt. */
    Debug{} << "Loading" << importer.textureCount() << "textures";
    mTextures.clear();
    mTextures.reserve(importer.textureCount());

    for (uint32_t i = 0; i != importer.textureCount(); ++i) {
        auto textureData = std::optional<Trade::TextureData>{importer.texture(i)};
        if (!textureData || textureData->type() != Trade::TextureType::Texture2D) {
            Warning{} << "Cannot load texture" << i << importer.textureName(i);
            continue;
        }

        auto imageData = std::optional<Trade::ImageData2D>{importer.image2D(textureData->image())};
        if (!imageData) {
            Warning{} << "Cannot load image" << textureData->image() << importer.image2DName(textureData->image());
            continue;
        }

        /* Configure the texture */
        GL::Texture2D texture;
        texture.setMagnificationFilter(textureData->magnificationFilter())
            .setMinificationFilter(textureData->minificationFilter(), textureData->mipmapFilter())
            .setWrapping(textureData->wrapping().xy());

        loadImage(texture, *imageData);
        mTextures.push_back({std::move(texture)});
    }
}

void MaterialCache::LoadMaterials(Magnum::Trade::AbstractImporter& importer) {
    /* Load all materials. Materials that fail to load will be NullOpt. The
       data will be stored directly in objects later, so save them only
       temporarily. */
    Debug{} << "Loading" << importer.materialCount() << "materials";
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
            Warning{} << "Cannot load material" << i << importer.materialName(i);
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
}  // namespace kitgui