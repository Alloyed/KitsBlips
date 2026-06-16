#include <Magnum/GL/Texture.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/PixelFormat.h>

namespace kitgui {

inline void LoadImageToTexture(const Magnum::Trade::ImageData2D& image, Magnum::GL::Texture2D& texture) {
    using namespace Corrade;
    using namespace Magnum;
    if (!image.isCompressed()) {
        Containers::Array<char> usedImageStorage;
        ImageView2D usedImage = image;
        const uint32_t channelCount = Magnum::pixelFormatChannelCount(image.format());
        if (channelCount == 1) {
            texture.setSwizzle<'r', 'r', 'r', '1'>();
        } else if (channelCount == 2) {
            texture.setSwizzle<'r', 'r', 'r', 'g'>();
        }
        GL::TextureFormat format{};
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
            default: {
                /*
                kitgui::log::error(
                    fmt::format("cannot load image of format {} ", static_cast<uint32_t>(usedImage.format())));
                */
                return;
            }
        }
        int32_t levels = static_cast<int32_t>(std::log2<int32_t>(usedImage.size().max()) + 1);
        texture.setStorage(levels, format, usedImage.size()).setSubImage(0, {}, usedImage).generateMipmap();
    } else {
        GL::TextureFormat format{};
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
            case CompressedPixelFormat::Astc12x12RGBAF: {
                /*
                kitgui::log::error(fmt::format("cannot load image of compressed format {} ",
                                               static_cast<uint32_t>(image.compressedFormat())));
                */
                return;
            }
            default:
                format = GL::textureFormat(image.compressedFormat());
        }
        texture.setStorage(1, format, image.size()).setCompressedSubImage(0, {}, image);
    }
}
}