#include "gfx/gltfScene.h"

// enable conversion to stl
#include <Corrade/Containers/OptionalStl.h>
#include <Corrade/Containers/PairStl.h>
#include <Corrade/Containers/StringStl.h>
// done

// Corrade is magnum's internal "base layer". where possible replace with stl and etl
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/BitArray.h>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Containers/Triple.h>
#include <Corrade/Utility/Algorithms.h>

#include <Magnum/Animation/Player.h>
#include <Magnum/Animation/Track.h>
#include <Magnum/GL/Context.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Extensions.h>
#include <Magnum/GL/GL.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/ImageView.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/CubicHermite.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Math/Quaternion.h>
#include <Magnum/MeshTools/Compile.h>
#include <Magnum/MeshTools/GenerateIndices.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/SceneGraph/Camera.h>
#include <Magnum/SceneGraph/Drawable.h>
#include <Magnum/SceneGraph/Scene.h>
#include <Magnum/SceneGraph/TranslationRotationScalingTransformation3D.h>
#include <Magnum/Shaders/FlatGL.h>
#include <Magnum/Shaders/MeshVisualizerGL.h>
#include <Magnum/Shaders/PhongGL.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/AnimationData.h>
#include <Magnum/Trade/CameraData.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/LightData.h>
#include <Magnum/Trade/MaterialData.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/TextureData.h>

#include <cassert>
#include <chrono>
#include <format>
#include <memory>
#include <optional>
#include <string>

using namespace Magnum;
using namespace Magnum::Math::Literals;
using namespace Magnum::Math::Literals::ColorLiterals;

using Object3D = Magnum::SceneGraph::Object<Magnum::SceneGraph::TranslationRotationScalingTransformation3D>;
using Scene3D = Magnum::SceneGraph::Scene<Magnum::SceneGraph::TranslationRotationScalingTransformation3D>;

namespace {
struct MeshInfo {
    std::optional<Magnum::GL::Mesh> mesh;
    uint32_t attributes;
    uint32_t vertices;
    uint32_t primitives;
    uint32_t objectIdCount;
    size_t size;
    std::string debugName;
    bool hasTangents, hasSeparateBitangents;
};

struct LightInfo {
    std::optional<Magnum::Trade::LightData> light;
    std::string debugName;
};

struct TextureInfo {
    std::optional<Magnum::GL::Texture2D> texture;
};

struct MaterialInfo {
    std::optional<Magnum::Trade::MaterialData> raw;
    const Magnum::Trade::PhongMaterialData* Phong() {
        return raw.has_value() && raw->types() & Trade::MaterialType::Phong ? &(raw->as<Trade::PhongMaterialData>())
                                                                            : nullptr;
    }
};

struct ObjectInfo {
    Object3D* object;
    std::string debugName;
    uint32_t meshId{0xffffffffu};
    uint32_t lightId{0xffffffffu};
    uint32_t childCount;
};

void loadImage(GL::Texture2D& texture, const Trade::ImageData2D& image) {
    if (!image.isCompressed()) {
        /* Single-channel images are probably meant to represent grayscale,
           two-channel grayscale + alpha. Probably, there's no way to know, but
           given we're using them for *colors*, it makes more sense than
           displaying them just red or red+green. */
        Containers::Array<char> usedImageStorage;
        ImageView2D usedImage = image;
        const uint32_t channelCount = pixelFormatChannelCount(image.format());
        if (channelCount == 1 || channelCount == 2) {
            /* Available in GLES 3 always */
            if (GL::Context::current().isExtensionSupported<GL::Extensions::ARB::texture_swizzle>()) {
                if (channelCount == 1)
                    texture.setSwizzle<'r', 'r', 'r', '1'>();
                else if (channelCount == 2)
                    texture.setSwizzle<'r', 'r', 'r', 'g'>();
            } else {
                /* Without texture swizzle support, allocate a copy of the image
                   and expand the channels manually */
                /** @todo make this a utility in TextureTools, with the channel
                    expansion being an optimized routine in Math/PackingBatch.h */
                const PixelFormat imageFormat =
                    pixelFormat(image.format(), channelCount == 2 ? 4 : 3, isPixelFormatSrgb(image.format()));
                Debug{} << "Texture swizzle not supported, expanding a" << image.format() << "image to" << imageFormat;

                /* Pad to four-byte rows to not have to use non-optimal
                   alignment */
                const std::size_t rowStride = 4 * ((pixelFormatSize(imageFormat) * image.size().x() + 3) / 4);
                usedImageStorage = Containers::Array<char>{NoInit, std::size_t(rowStride * image.size().y())};
                const MutableImageView2D usedMutableImage{imageFormat, image.size(), usedImageStorage};
                usedImage = usedMutableImage;

                /* Create 4D pixel views (rows, pixels, channels, channel
                   bytes) */
                const std::size_t channelSize = pixelFormatSize(pixelFormatChannelFormat(imageFormat));
                const std::size_t dstChannelCount = pixelFormatChannelCount(imageFormat);
                const Containers::StridedArrayView4D<const char> src =
                    image.pixels().expanded<2>(Containers::Size2D{channelCount, channelSize});
                const Containers::StridedArrayView4D<char> dst =
                    usedMutableImage.pixels().expanded<2>(Containers::Size2D{dstChannelCount, channelSize});

                /* Broadcast the red channel of the input to RRR and copy to
                   the RGB channels of the output */
                Utility::copy(src.exceptSuffix({0, 0, channelCount == 2 ? 1 : 0, 0}).broadcasted<2>(3),
                              dst.exceptSuffix({0, 0, channelCount == 2 ? 1 : 0, 0}));
                /* If there's an alpha channel, copy it over as well */
                if (channelCount == 2)
                    Utility::copy(src.exceptPrefix({0, 0, 1, 0}), dst.exceptPrefix({0, 0, 3, 0}));
            }
        }

        /* Whitelist only things we *can* display */
        /** @todo signed formats, exposure knob for float formats */
        GL::TextureFormat format;
        switch (usedImage.format()) {
            case PixelFormat::R8Unorm:
            case PixelFormat::RG8Unorm:
            /* can't really do sRGB R/RG as there are no widely available
               desktop extensions :( */
            case PixelFormat::RGB8Unorm:
            case PixelFormat::RGB8Srgb:
            case PixelFormat::RGBA8Unorm:
            case PixelFormat::RGBA8Srgb:
            /* I guess we can try using 16-bit formats even though our displays
               won't be able to show all the detail */
            case PixelFormat::R16Unorm:
            case PixelFormat::RG16Unorm:
            case PixelFormat::RGB16Unorm:
            case PixelFormat::RGBA16Unorm:
            /* Floating point is fine too */
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
        /* Blacklist things we *cannot* display */
        GL::TextureFormat format;
        switch (image.compressedFormat()) {
            /** @todo signed formats, float formats */
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
        /** @todo mip level loading */
    }
}

class FlatDrawable : public SceneGraph::Drawable3D {
   public:
    explicit FlatDrawable(Object3D& object,
                          Shaders::FlatGL3D& shader,
                          GL::Mesh& mesh,
                          uint32_t objectId,
                          const Color4& color,
                          const Vector3& scale,
                          SceneGraph::DrawableGroup3D& group)
        : SceneGraph::Drawable3D{object, &group},
          mShader(shader),
          mMesh(mesh),
          mObjectId{objectId},
          mColor{color},
          mScale{scale} {}

   private:
    void draw(const Matrix4& transformationMatrix, SceneGraph::Camera3D& camera) override {
        /* Override the inherited scale, if requested */
        Matrix4 transformation;
        if (mScale == mScale)
            transformation = Matrix4::from(transformationMatrix.rotationShear(), transformationMatrix.translation()) *
                             Matrix4::scaling(Vector3{mScale});
        else
            transformation = transformationMatrix;

        mShader.setColor(mColor)
            .setTransformationProjectionMatrix(camera.projectionMatrix() * transformation)
            .setObjectId(mObjectId);

        mShader.draw(mMesh);
    }

    Shaders::FlatGL3D& mShader;
    GL::Mesh& mMesh;
    uint32_t mObjectId;
    Color4 mColor;
    Vector3 mScale;
};

class PhongDrawable : public SceneGraph::Drawable3D {
   public:
    explicit PhongDrawable(Object3D& object,
                           Shaders::PhongGL& shader,
                           GL::Mesh& mesh,
                           uint32_t objectId,
                           const Color4& color,
                           GL::Texture2D* diffuseTexture,
                           GL::Texture2D* normalTexture,
                           Float normalTextureScale,
                           Float alphaMask,
                           const Matrix3& textureMatrix,
                           const bool& shadeless,
                           SceneGraph::DrawableGroup3D& group)
        : SceneGraph::Drawable3D{object, &group},
          mShader(shader),
          mMesh(mesh),
          mObjectId{objectId},
          mColor{color},
          mDiffuseTexture{diffuseTexture},
          mNormalTexture{normalTexture},
          mNormalTextureScale{normalTextureScale},
          mAlphaMask{alphaMask},
          mTextureMatrix{textureMatrix},
          mShadeless{shadeless} {}

    explicit PhongDrawable(Object3D& object,
                           Shaders::PhongGL& shader,
                           GL::Mesh& mesh,
                           uint32_t objectId,
                           const Color4& color,
                           const bool& shadeless,
                           SceneGraph::DrawableGroup3D& group)
        : SceneGraph::Drawable3D{object, &group},
          mShader(shader),
          mMesh(mesh),
          mObjectId{objectId},
          mColor{color},
          mDiffuseTexture{nullptr},
          mNormalTexture{nullptr},
          mAlphaMask{0.5f},
          mShadeless{shadeless} {}

   private:
    void draw(const Matrix4& transformationMatrix, SceneGraph::Camera3D& camera) override {
        Matrix4 usedTransformationMatrix{NoInit};
        usedTransformationMatrix = transformationMatrix;

        mShader.setTransformationMatrix(usedTransformationMatrix)
            .setNormalMatrix(usedTransformationMatrix.normalMatrix())
            .setProjectionMatrix(camera.projectionMatrix())
            .setObjectId(mObjectId);

        if (mDiffuseTexture)
            mShader.bindAmbientTexture(*mDiffuseTexture).bindDiffuseTexture(*mDiffuseTexture);
        if (mNormalTexture)
            mShader.bindNormalTexture(*mNormalTexture).setNormalTextureScale(mNormalTextureScale);

        if (mShadeless)
            mShader.setAmbientColor(mColor).setDiffuseColor(0x00000000_rgbaf).setSpecularColor(0x00000000_rgbaf);
        else
            mShader.setAmbientColor(mColor * 0.06f).setDiffuseColor(mColor).setSpecularColor(0x11111100_rgbaf);

        if (mShader.flags() & Shaders::PhongGL::Flag::TextureTransformation)
            mShader.setTextureMatrix(mTextureMatrix);
        if (mShader.flags() & Shaders::PhongGL::Flag::AlphaMask)
            mShader.setAlphaMask(mAlphaMask);
        if (mShader.flags() & Shaders::PhongGL::Flag::DoubleSided)
            GL::Renderer::disable(GL::Renderer::Feature::FaceCulling);

        mShader.draw(mMesh);

        if (mShader.flags() & Shaders::PhongGL::Flag::DoubleSided)
            GL::Renderer::enable(GL::Renderer::Feature::FaceCulling);
    }

    Shaders::PhongGL& mShader;
    GL::Mesh& mMesh;
    uint32_t mObjectId;
    Color4 mColor;
    GL::Texture2D* mDiffuseTexture;
    GL::Texture2D* mNormalTexture;
    Float mNormalTextureScale;
    Float mAlphaMask;
    Matrix3 mTextureMatrix;
    const bool& mShadeless;
};

class LightDrawable : public SceneGraph::Drawable3D {
   public:
    explicit LightDrawable(Object3D& object,
                           bool directional,
                           std::vector<Vector4>& positions,
                           SceneGraph::DrawableGroup3D& group)
        : SceneGraph::Drawable3D{object, &group}, _directional{directional}, _positions(positions) {}

   private:
    void draw(const Matrix4& transformationMatrix, SceneGraph::Camera3D&) override {
        _positions.push_back(_directional ? Vector4{transformationMatrix.backward(), 0.0f}
                                          : Vector4{transformationMatrix.translation(), 1.0f});
    }

    bool _directional;
    std::vector<Vector4>& _positions;
};

}  // namespace

namespace kitgui {
struct GltfSceneImpl {
    void Load(Magnum::Trade::AbstractImporter& importer, std::string_view debugName);

    Shaders::FlatGL3D& flatShader(Shaders::FlatGL3D::Flags flags);
    Shaders::PhongGL& phongShader(Shaders::PhongGL::Flags flags);
    void updateLightColorBrightness();

    std::vector<MeshInfo> mMeshes;
    Magnum::SceneGraph::DrawableGroup3D mDrawables;

    std::vector<MaterialInfo> mMaterials;
    std::vector<TextureInfo> mTextures;

    std::vector<LightInfo> mLights;
    size_t mLightCount{};
    bool mShadeless{false};
    float mBrightness{0.5f};

    std::vector<Vector4> mLightPositions;
    std::vector<Color3> mLightColors;
    Magnum::SceneGraph::DrawableGroup3D mLightDrawables;

    Scene3D mScene;
    std::vector<ObjectInfo> mSceneObjects;
    Object3D* mCameraObject{};
    SceneGraph::Camera3D* mCamera;

    Animation::Player<std::chrono::nanoseconds, Float> mPlayer;

    /* Indexed by Shaders::FlatGL3D::Flags, PhongGL::Flags or
   MeshVisualizerGL3D::Flags but cast to an UnsignedInt because I
   refuse to deal with the std::hash crap. */
    std::unordered_map<UnsignedInt, Shaders::FlatGL3D> mFlatShaders;
    std::unordered_map<UnsignedInt, Shaders::PhongGL> mPhongShaders;
    std::unordered_map<UnsignedInt, Shaders::MeshVisualizerGL3D> mMeshVisualizerShaders;
};

GltfScene::GltfScene() : mImpl(std::make_unique<GltfSceneImpl>()) {}

Shaders::FlatGL3D& GltfSceneImpl::flatShader(Shaders::FlatGL3D::Flags flags) {
    auto found = mFlatShaders.find(enumCastUnderlyingType(flags));
    if (found == mFlatShaders.end()) {
        Shaders::FlatGL3D::Configuration configuration;
        configuration.setFlags(Shaders::FlatGL3D::Flag::ObjectId | flags);
        found = mFlatShaders.emplace(enumCastUnderlyingType(flags), Shaders::FlatGL3D{configuration}).first;
    }
    return found->second;
}

Shaders::PhongGL& GltfSceneImpl::phongShader(Shaders::PhongGL::Flags flags) {
    auto found = mPhongShaders.find(enumCastUnderlyingType(flags));
    if (found == mPhongShaders.end()) {
        Shaders::PhongGL::Configuration configuration;
        configuration.setFlags(Shaders::PhongGL::Flag::ObjectId | flags).setLightCount(mLightCount ? mLightCount : 3);
        found = mPhongShaders.emplace(enumCastUnderlyingType(flags), Shaders::PhongGL{configuration}).first;

        found->second.setSpecularColor(0x11111100_rgbaf).setShininess(80.0f);
    }
    return found->second;
}

void GltfSceneImpl::updateLightColorBrightness() {
    Containers::Array<Color3> lightColorsBrightness{NoInit, mLightColors.size()};
    for (UnsignedInt i = 0; i != lightColorsBrightness.size(); ++i) {
        lightColorsBrightness[i] = mLightColors[i] * mBrightness;
    }
    for (auto& shader : mPhongShaders) {
        shader.second.setLightColors(lightColorsBrightness);
    }
}

void GltfScene::Load(Magnum::Trade::AbstractImporter& importer, std::string_view debugName) {
    mImpl->Load(importer, debugName);
}

void GltfSceneImpl::Load(Magnum::Trade::AbstractImporter& importer, std::string_view debugName) {
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

    /* Load all lights. Lights that fail to load will be NullOpt, saving the
       whole imported data so we can populate the selection info later. */
    Debug{} << "Loading" << importer.lightCount() << "lights";
    mLights.clear();
    mLights.reserve(importer.lightCount());
    for (uint32_t i = 0; i != importer.lightCount(); ++i) {
        LightInfo lightInfo;
        lightInfo.debugName = importer.lightName(i);
        if (lightInfo.debugName.empty()) {
            lightInfo.debugName = std::format("{}", i);
        }

        auto light = std::optional<Trade::LightData>{importer.light(i)};
        if (!light) {
            Warning{} << "Cannot load light" << i << importer.lightName(i);
            continue;
        }
        mLights.push_back(std::move(lightInfo));
    }

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

    /* Load all meshes. Meshes that fail to load will be NullOpt. Remember
       which have vertex colors, so in case there's no material we can use that
       instead. */
    Debug{} << "Loading" << importer.meshCount() << "meshes";
    mMeshes.clear();
    mMeshes.reserve(importer.meshCount());
    Containers::BitArray hasVertexColors{ValueInit, importer.meshCount()};
    for (uint32_t i = 0; i != importer.meshCount(); ++i) {
        mMeshes.push_back({});
        MeshInfo& mesh = mMeshes.back();
        auto meshData = std::optional<Trade::MeshData>(importer.mesh(i));
        if (!meshData) {
            Warning{} << "Cannot load mesh" << i << importer.meshName(i);
            continue;
        }

        std::string meshName = importer.meshName(i);
        if (meshName.empty())
            meshName = std::format("{}", i);

        /* Disable warnings on custom attributes, as we printed them with
           actual string names below. Generate normals for triangle meshes
           (and don't do anything for line/point meshes, there it makes no
           sense). */
        MeshTools::CompileFlags flags = MeshTools::CompileFlag::NoWarnOnCustomAttributes;
        if ((meshData->primitive() == MeshPrimitive::Triangles ||
             meshData->primitive() == MeshPrimitive::TriangleStrip ||
             meshData->primitive() == MeshPrimitive::TriangleFan) &&
            !meshData->attributeCount(Trade::MeshAttribute::Normal) &&
            meshData->hasAttribute(Trade::MeshAttribute::Position) &&
            meshData->attributeFormat(Trade::MeshAttribute::Position) == VertexFormat::Vector3) {
            /* If the mesh is a triangle strip/fan, convert to indexed
               triangles first. If the strip/fan is indexed, we can attempt to
               generate smooth normals. If it's not, generate flat ones as
               otherwise the smoothing would be only along the strip and not at
               the seams, looking weird. */
            if (meshData->primitive() == MeshPrimitive::TriangleStrip ||
                meshData->primitive() == MeshPrimitive::TriangleFan) {
                if (meshData->isIndexed()) {
                    Debug{} << "Mesh" << meshName.c_str()
                            << "doesn't have normals, generating smooth ones using information from the index "
                               "buffer for a"
                            << meshData->primitive();
                    flags |= MeshTools::CompileFlag::GenerateSmoothNormals;
                } else {
                    Debug{} << "Mesh" << meshName.c_str() << "doesn't have normals, generating flat ones for a"
                            << meshData->primitive();
                    flags |= MeshTools::CompileFlag::GenerateFlatNormals;
                }

                meshData = MeshTools::generateIndices(*Utility::move(meshData));

                /* Otherwise prefer smooth normals, if we have an index buffer
                   telling us neighboring faces */
            } else if (meshData->isIndexed()) {
                Debug{} << "Mesh" << meshName.c_str()
                        << "doesn't have normals, generating smooth ones using information from the index buffer";
                flags |= MeshTools::CompileFlag::GenerateSmoothNormals;
            } else {
                Debug{} << "Mesh" << meshName.c_str() << "doesn't have normals, generating flat ones";
                flags |= MeshTools::CompileFlag::GenerateFlatNormals;
            }
        }

        /* Print messages about ignored attributes / levels */
        for (uint32_t j = 0; j != meshData->attributeCount(); ++j) {
            const Trade::MeshAttribute name = meshData->attributeName(j);
            if (Trade::isMeshAttributeCustom(name)) {
                if (const std::string stringName = importer.meshAttributeName(name); !stringName.empty())
                    Warning{} << "Mesh" << meshName.c_str() << "has a custom mesh attribute" << stringName.c_str()
                              << Debug::nospace << ", ignoring";
                else
                    Warning{} << "Mesh" << meshName.c_str() << "has a custom mesh attribute" << name << Debug::nospace
                              << ", ignoring";
                continue;
            }

            const VertexFormat format = meshData->attributeFormat(j);
            if (isVertexFormatImplementationSpecific(format))
                Warning{} << "Mesh" << meshName.c_str() << "has" << name << "of format" << format << Debug::nospace
                          << ", ignoring";
        }
        const uint32_t meshLevels = importer.meshLevelCount(i);
        if (meshLevels > 1)
            Warning{} << "Mesh" << meshName.c_str() << "has" << meshLevels - 1 << "additional mesh levels, ignoring";

        hasVertexColors.set(i, meshData->hasAttribute(Trade::MeshAttribute::Color));

        /* Save metadata, compile the mesh */
        mesh.attributes = meshData->attributeCount();
        mesh.vertices = meshData->vertexCount();
        mesh.size = meshData->vertexData().size();
        if (meshData->isIndexed()) {
            mesh.primitives = MeshTools::primitiveCount(meshData->primitive(), meshData->indexCount());
            mesh.size += meshData->indexData().size();
        } else
            mesh.primitives = MeshTools::primitiveCount(meshData->primitive(), meshData->vertexCount());
        /* Needed for a warning when using a mesh with no tangents with a
           normal map (as, unlike with normals, we have no builtin way to
           generate tangents right now) */
        mesh.hasTangents = meshData->hasAttribute(Trade::MeshAttribute::Tangent);
        /* Needed to decide how to visualize tangent space */
        mesh.hasSeparateBitangents = meshData->hasAttribute(Trade::MeshAttribute::Bitangent);

        mesh.objectIdCount = 0;
        if (meshData->hasAttribute(Trade::MeshAttribute::ObjectId)) {
            for (const auto id : meshData->objectIdsAsArray()) {
                mesh.objectIdCount = std::max(mesh.objectIdCount, id);
            }
        }
        mesh.mesh = MeshTools::compile(*meshData, flags);
        mesh.debugName = meshName;
    }

    /* Load the scene. Save the object pointers in an array for easier mapping
       of animations later. */
    uint32_t id = importer.defaultScene() == -1 ? 0 : importer.defaultScene();
    Debug{} << "Loading scene" << id << importer.sceneName(id);

    auto scene = std::optional<Trade::SceneData>(importer.scene(id));
    if (!scene || !scene->is3D() || !scene->hasField(Trade::SceneField::Parent)) {
        Error{} << "Cannot load the scene, aborting";
        return;
    }

    /* Allocate objects that are part of the hierarchy and fill their
       implicit info */
    mSceneObjects.clear();
    mSceneObjects.resize(scene->mappingBound());
    const auto parents = scene->parentsAsArray();
    for (const auto& parent : parents) {
        const uint32_t objectId = parent.first();
        auto& objectInfo = mSceneObjects[objectId];

        objectInfo.object = new Object3D{};
        objectInfo.debugName = importer.objectName(objectId);
        if (objectInfo.debugName.empty()) {
            objectInfo.debugName = std::format("object {}", objectId);
        }
    }

    /* Assign parent references, separately because there's no guarantee
       that a parent was allocated already when it's referenced */
    for (const auto& parent : parents) {
        const uint32_t objectId = parent.first();
        auto& objectInfo = mSceneObjects[objectId];

        objectInfo.object->setParent(parent.second() == -1 ? &mScene : mSceneObjects[parent.second()].object);

        if (parent.second() != -1)
            ++mSceneObjects[parent.second()].childCount;
    }

    /* Set transformations. Objects that are not part of the hierarchy are
       ignored, objects that have no transformation entry retain an
       identity transformation. Assign TRS first, if available, and then
       fall back to matrices for the rest. */
    {
        Containers::BitArray hasTrs{ValueInit, std::size_t(scene->mappingBound())};
        if (scene->hasField(Trade::SceneField::Translation) || scene->hasField(Trade::SceneField::Rotation) ||
            scene->hasField(Trade::SceneField::Scaling)) {
            for (const auto& trs : scene->translationsRotationsScalings3DAsArray()) {
                hasTrs.set(trs.first());
                if (Object3D* object = mSceneObjects[trs.first()].object)
                    (*object)
                        .setTranslation(trs.second().first())
                        .setRotation(trs.second().second())
                        .setScaling(trs.second().third());
            }
        }
        for (const auto& transformation : scene->transformations3DAsArray()) {
            if (hasTrs[transformation.first()])
                continue;
            if (Object3D* object = mSceneObjects[transformation.first()].object)
                object->setTransformation(transformation.second());
        }
    }

    /* Import all lights so we know which shaders to instantiate */
    if (scene->hasField(Trade::SceneField::Light)) {
        for (const auto& lightReference : scene->lightsAsArray()) {
            const uint32_t objectId = lightReference.first();
            auto& objectInfo = mSceneObjects[objectId];
            const uint32_t lightId = lightReference.second();
            Object3D* object = objectInfo.object;
            const auto& light = mLights[lightId].light;
            if (!object || !light)
                continue;

            ++mLightCount;

            /* Save the light pointer as well, so we know what to print for
               object selection. Lights have their own info text, so not
               setting the type. */
            /** @todo this doesn't handle multi-light objects */
            objectInfo.lightId = lightId;

            /* Add a light drawable, which puts correct camera-relative
               position to lightPositions. Light colors don't change so
               add that directly. */
            new LightDrawable{*object, light->type() == Trade::LightType::Directional ? true : false, mLightPositions,
                              mLightDrawables};
            mLightColors.push_back(light->color() * light->intensity());
        }
    }

    /* Import camera references, the first camera will be treated as the
       default one */
    if (scene->hasField(Trade::SceneField::Camera)) {
        for (const auto& cameraReference : scene->camerasAsArray()) {
            const uint32_t objectId = cameraReference.first();
            const auto& objectInfo = mSceneObjects[objectId];
            Object3D* const object = objectInfo.object;
            if (!object)
                continue;

            if (cameraReference.second() == 0) {
                mCameraObject = object;
            }
        }
    }

    /* Add drawables for objects that have a mesh, again ignoring objects
       that are not part of the hierarchy. There can be multiple mesh
       assignments for one object, simply add one drawable for each. */
    if (scene->hasField(Trade::SceneField::Mesh)) {
        for (const auto& meshMaterial : scene->meshesMaterialsAsArray()) {
            const uint32_t objectId = meshMaterial.first();
            auto& objectInfo = mSceneObjects[objectId];
            Object3D* const object = objectInfo.object;
            const uint32_t meshId = meshMaterial.second().first();
            const Int materialId = meshMaterial.second().second();
            std::optional<GL::Mesh>& mesh = mMeshes[meshId].mesh;
            if (!object || !mesh)
                continue;

            /* Save the mesh pointer as well, so we know what to draw for object
               selection */
            objectInfo.meshId = meshId;

            Shaders::PhongGL::Flags flags;
            if (hasVertexColors[meshId])
                flags |= Shaders::PhongGL::Flag::VertexColor;
            if (mMeshes[meshId].hasSeparateBitangents)
                flags |= Shaders::PhongGL::Flag::Bitangent;

            /* Material not available / not loaded. If the mesh has vertex
               colors, use that, otherwise apply a default material; use a flat
               shader for lines / points */
            if (materialId == -1 || !mMaterials[materialId].raw.has_value()) {
                if (mesh->primitive() == GL::MeshPrimitive::Triangles ||
                    mesh->primitive() == GL::MeshPrimitive::TriangleStrip ||
                    mesh->primitive() == GL::MeshPrimitive::TriangleFan) {
                    new PhongDrawable{*object,       phongShader(flags), *mesh,     objectId,
                                      0xffffff_rgbf, mShadeless,         mDrawables};
                } else {
                    new FlatDrawable{*object,
                                     flatShader((hasVertexColors[meshId] ? Shaders::FlatGL3D::Flag::VertexColor
                                                                         : Shaders::FlatGL3D::Flags{})),
                                     *mesh,
                                     objectId,
                                     0xffffff_rgbf,
                                     Vector3{Constants::nan()},
                                     mDrawables};
                }

                /* Material available */
            } else {
                const Trade::PhongMaterialData& material = *mMaterials[materialId].Phong();

                if (material.isDoubleSided())
                    flags |= Shaders::PhongGL::Flag::DoubleSided;

                /* Textured material. If the texture failed to load, again just
                   use a default-colored material. */
                GL::Texture2D* diffuseTexture = nullptr;
                GL::Texture2D* normalTexture = nullptr;
                Float normalTextureScale = 1.0f;
                if (material.hasAttribute(Trade::MaterialAttribute::DiffuseTexture)) {
                    std::optional<GL::Texture2D>& texture = mTextures[material.diffuseTexture()].texture;
                    if (texture) {
                        diffuseTexture = &*texture;
                        flags |= Shaders::PhongGL::Flag::AmbientTexture | Shaders::PhongGL::Flag::DiffuseTexture;
                        if (material.hasTextureTransformation())
                            flags |= Shaders::PhongGL::Flag::TextureTransformation;
                        if (material.alphaMode() == Trade::MaterialAlphaMode::Mask)
                            flags |= Shaders::PhongGL::Flag::AlphaMask;
                    }
                }

                /* Normal textured material. If the textures fail to load,
                   again just use a default-colored material. */
                if (material.hasAttribute(Trade::MaterialAttribute::NormalTexture)) {
                    std::optional<GL::Texture2D>& texture = mTextures[material.normalTexture()].texture;
                    /* If there are no tangents, the mesh would render all
                       black. Ignore the normal map in that case. */
                    /** @todo generate tangents instead once we have the algo */
                    if (!mMeshes[meshId].hasTangents) {
                        Warning{} << "Mesh" << mMeshes[meshId].debugName.c_str()
                                  << "doesn't have tangents and Magnum can't generate them yet, ignoring a "
                                     "normal map";
                    } else if (texture) {
                        normalTexture = &*texture;
                        normalTextureScale = material.normalTextureScale();
                        flags |= Shaders::PhongGL::Flag::NormalTexture;
                        if (material.hasTextureTransformation())
                            flags |= Shaders::PhongGL::Flag::TextureTransformation;
                    }
                }

                if (material.alphaMode() == Trade::MaterialAlphaMode::Blend) {
                    // todo, transparent materials
                    continue;
                }

                new PhongDrawable{*object,
                                  phongShader(flags),
                                  *mesh,
                                  objectId,
                                  material.diffuseColor(),
                                  diffuseTexture,
                                  normalTexture,
                                  normalTextureScale,
                                  material.alphaMask(),
                                  material.commonTextureMatrix(),
                                  mShadeless,
                                  mDrawables};
            }
        }
    }

    /* Create a camera object in case it wasn't present in the scene already */
    if (!mCameraObject) {
        mCameraObject = new Object3D{&mScene};
        mCameraObject->translate(Vector3::zAxis(5.0f));
    }

    /* Create default camera-relative lights in case they weren't present in
       the scene already. Don't add any visualization for those. */
    if (mLightCount == 0) {
        mLightCount = 3;

        Object3D* first = new Object3D{mCameraObject};
        first->translate({10.0f, 10.0f, 10.0f});
        new LightDrawable{*first, true, mLightPositions, mLightDrawables};

        Object3D* second = new Object3D{mCameraObject};
        first->translate(Vector3{-5.0f, -5.0f, 10.0f} * 100.0f);
        new LightDrawable{*second, true, mLightPositions, mLightDrawables};

        Object3D* third = new Object3D{mCameraObject};
        third->translate(Vector3{0.0f, 10.0f, -10.0f} * 100.0f);
        new LightDrawable{*third, true, mLightPositions, mLightDrawables};

        mLightColors = {0xffffff_rgbf, 0xffcccc_rgbf, 0xccccff_rgbf};
    }

    /* Initialize light colors for all instantiated shaders */
    updateLightColorBrightness();

    /* Basic camera setup */
    (*(mCamera = new SceneGraph::Camera3D{*mCameraObject}))
        .setAspectRatioPolicy(SceneGraph::AspectRatioPolicy::Extend)
        .setProjectionMatrix(Matrix4::perspectiveProjection(75.0_degf, 1.0f, 0.01f, 1000.0f))
        .setViewport(GL::defaultFramebuffer.viewport().size());

    /* Use the settings with parameters of the camera in the model, if any,
       otherwise just used the hardcoded setup from above */
    if (importer.cameraCount()) {
        auto camera = std::optional<Trade::CameraData>(importer.camera(0));
        if (camera)
            mCamera->setProjectionMatrix(
                Matrix4::perspectiveProjection(camera->fov(), 1.0f, camera->near(), camera->far()));
    }

    /* Import animations */
    if (importer.animationCount())
        Debug{} << "Importing the first animation out of" << importer.animationCount();
    for (uint32_t i = 0; i != importer.animationCount(); ++i) {
        auto animation = importer.animation(i);
        if (!animation) {
            Warning{} << "Cannot load animation" << i << importer.animationName(i);
            continue;
        }

        for (uint32_t j = 0; j != animation->trackCount(); ++j) {
            if (animation->trackTarget(j) >= mSceneObjects.size() || !mSceneObjects[animation->trackTarget(j)].object) {
                continue;
            }

            Object3D& animatedObject = *mSceneObjects[animation->trackTarget(j)].object;

            if (animation->trackTargetName(j) == Trade::AnimationTrackTarget::Translation3D) {
                const auto callback = [](Float, const Vector3& translation, Object3D& object) {
                    object.setTranslation(translation);
                };
                if (animation->trackType(j) == Trade::AnimationTrackType::CubicHermite3D) {
                    mPlayer.addWithCallback(animation->track<CubicHermite3D>(j), callback, animatedObject);
                } else {
                    assert(animation->trackType(j) == Trade::AnimationTrackType::Vector3);
                    mPlayer.addWithCallback(animation->track<Vector3>(j), callback, animatedObject);
                }
            } else if (animation->trackTargetName(j) == Trade::AnimationTrackTarget::Rotation3D) {
                const auto callback = [](Float, const Quaternion& rotation, Object3D& object) {
                    object.setRotation(rotation);
                };
                if (animation->trackType(j) == Trade::AnimationTrackType::CubicHermiteQuaternion) {
                    mPlayer.addWithCallback(animation->track<CubicHermiteQuaternion>(j), callback, animatedObject);
                } else {
                    assert(animation->trackType(j) == Trade::AnimationTrackType::Quaternion);
                    mPlayer.addWithCallback(animation->track<Quaternion>(j), callback, animatedObject);
                }
            } else if (animation->trackTargetName(j) == Trade::AnimationTrackTarget::Scaling3D) {
                const auto callback = [](Float, const Vector3& scaling, Object3D& object) {
                    object.setScaling(scaling);
                };
                if (animation->trackType(j) == Trade::AnimationTrackType::CubicHermite3D) {
                    mPlayer.addWithCallback(animation->track<CubicHermite3D>(j), callback, animatedObject);
                } else {
                    assert(animation->trackType(j) == Trade::AnimationTrackType::Vector3);
                    mPlayer.addWithCallback(animation->track<Vector3>(j), callback, animatedObject);
                }
            } else {
            }
        }
        //_data->animationData = animation->release();

        /* Load only the first animation at the moment */
        break;
    }
}
}  // namespace kitgui