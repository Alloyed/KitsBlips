#include "gfx/drawables.h"

#include <Magnum/GL/Renderer.h>
#include <Magnum/SceneGraph/Camera.h>
#include <Magnum/Shaders/FlatGL.h>
#include <Magnum/Shaders/PhongGL.h>
#include <Magnum/Trade/MaterialData.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <fmt/format.h>
#include <vector>
#include <imgui.h>

#include "gfx/materials.h"
#include "gfx/sceneGraph.h"
#include "log.h"

using namespace Magnum;
using namespace Magnum::Math::Literals;
using namespace Magnum::Math::Literals::ColorLiterals;

namespace kitgui {

FlatDrawable::FlatDrawable(Object3D& object,
                           Shaders::FlatGL3D& shader,
                           GL::Mesh& mesh,
                           uint32_t objectId,
                           const Color4& color,
                           const Vector3& scale,
                           SceneGraph::DrawableGroup3D& group)
    : BaseDrawable{object, &group},
      mShader(shader),
      mMesh(mesh),
      mObjectId{objectId},
      mColor{color},
      mScale{scale} {}

void FlatDrawable::draw(const Matrix4& transformationMatrix, SceneGraph::Camera3D& camera) {
    /* Override the inherited scale, if requested */
    Matrix4 transformation;
    if (mScale == mScale) {
        transformation = Matrix4::from(transformationMatrix.rotationShear(), transformationMatrix.translation()) *
                         Matrix4::scaling(Vector3{mScale});
    } else {
        // NaN
        transformation = transformationMatrix;
    }

    mShader.setColor(mColor)
        .setTransformationProjectionMatrix(camera.projectionMatrix() * transformation)
        .setObjectId(mObjectId);

    mShader.draw(mMesh);
}

void FlatDrawable::ImGui() {
    if(ImGui::TreeNode(this, "%d", mObjectId)) {
        ImGui::Text("Flat");
        ImVec4 c = {mColor.r(), mColor.g(), mColor.b(), mColor.a()};
        ImGui::ColorButton("color", c);
        ImGui::LabelText("scale", "%f, %f, %f", mScale.x(), mScale.y(), mScale.z());
        ImGui::TreePop();
    }
}

PhongDrawable::PhongDrawable(Object3D& object,
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
    : BaseDrawable{object, &group},
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

PhongDrawable::PhongDrawable(Object3D& object,
                             Shaders::PhongGL& shader,
                             GL::Mesh& mesh,
                             uint32_t objectId,
                             const Color4& color,
                             const bool& shadeless,
                             SceneGraph::DrawableGroup3D& group)
    : BaseDrawable{object, &group},
      mShader(shader),
      mMesh(mesh),
      mObjectId{objectId},
      mColor{color},
      mDiffuseTexture{nullptr},
      mNormalTexture{nullptr},
      mAlphaMask{0.5f},
      mShadeless{shadeless} {}

void PhongDrawable::draw(const Matrix4& transformationMatrix, SceneGraph::Camera3D& camera) {
    Matrix4 usedTransformationMatrix{NoInit};
    usedTransformationMatrix = transformationMatrix;

    mShader.setTransformationMatrix(usedTransformationMatrix)
        .setNormalMatrix(usedTransformationMatrix.normalMatrix())
        .setProjectionMatrix(camera.projectionMatrix())
        .setObjectId(mObjectId);

    if (mDiffuseTexture) {
        mShader.bindAmbientTexture(*mDiffuseTexture).bindDiffuseTexture(*mDiffuseTexture);
    }
    if (mNormalTexture) {
        mShader.bindNormalTexture(*mNormalTexture).setNormalTextureScale(mNormalTextureScale);
    }

    if (mShadeless) {
        mShader.setAmbientColor(mColor).setDiffuseColor(0x00000000_rgbaf).setSpecularColor(0x00000000_rgbaf);
    } else {
        mShader.setAmbientColor(mColor * 0.06f).setDiffuseColor(mColor).setSpecularColor(0x11111100_rgbaf);
    }

    if (mShader.flags() & Shaders::PhongGL::Flag::TextureTransformation) {
        mShader.setTextureMatrix(mTextureMatrix);
    }
    if (mShader.flags() & Shaders::PhongGL::Flag::AlphaMask) {
        mShader.setAlphaMask(mAlphaMask);
    }
    if (mShader.flags() & Shaders::PhongGL::Flag::DoubleSided) {
        GL::Renderer::disable(GL::Renderer::Feature::FaceCulling);
    }

    mShader.draw(mMesh);

    if (mShader.flags() & Shaders::PhongGL::Flag::DoubleSided) {
        GL::Renderer::enable(GL::Renderer::Feature::FaceCulling);
    }
}

void PhongDrawable::ImGui() {
    if(ImGui::TreeNode(this, "%d", mObjectId)) {
        ImGui::Text("PhongGL");
        ImVec4 c = {mColor.r(), mColor.g(), mColor.b(), mColor.a()};
        ImGui::ColorButton("color", c);
        ImGui::LabelText("hasDiffuse", "%s", mDiffuseTexture == nullptr ? "false" : "true");
        ImGui::LabelText("hasNormal", "%s", mNormalTexture == nullptr ? "false" : "true");
        ImGui::LabelText("normalTextureScale", "%f", mNormalTextureScale);
        ImGui::LabelText("alphamask", "%f", mAlphaMask);
        ImGui::LabelText("shadeless", "%s", mShadeless ? "true":"false");
        ImGui::TreePop();
    }
}

LightDrawable::LightDrawable(Object3D& object,
                             bool directional,
                             std::vector<Vector4>& positions,
                             SceneGraph::DrawableGroup3D& group)
    : BaseDrawable{object, &group}, mDirectional{directional}, mPositions(positions) {}

void LightDrawable::draw(const Matrix4& transformationMatrix, SceneGraph::Camera3D&) {
    mPositions.push_back(mDirectional ? Vector4{transformationMatrix.backward(), 0.0f}
                                      : Vector4{transformationMatrix.translation(), 1.0f});
}
void LightDrawable::ImGui() {
}


Shaders::FlatGL3D& DrawableCache::FlatShader(Shaders::FlatGL3D::Flags flags) {
    auto found = mFlatShaders.find(enumCastUnderlyingType(flags));
    if (found == mFlatShaders.end()) {
        Shaders::FlatGL3D::Configuration configuration;
        configuration.setFlags(Shaders::FlatGL3D::Flag::ObjectId | flags);
        found = mFlatShaders.emplace(enumCastUnderlyingType(flags), Shaders::FlatGL3D{configuration}).first;
    }
    return found->second;
}

Shaders::PhongGL& DrawableCache::PhongShader(Shaders::PhongGL::Flags flags, uint32_t lightCount) {
    auto found = mPhongShaders.find(enumCastUnderlyingType(flags));
    if (found == mPhongShaders.end()) {
        Shaders::PhongGL::Configuration configuration;
        configuration.setFlags(Shaders::PhongGL::Flag::ObjectId | flags).setLightCount(lightCount);
        found = mPhongShaders.emplace(enumCastUnderlyingType(flags), Shaders::PhongGL{configuration}).first;

        found->second.setSpecularColor(0x11111100_rgbaf).setShininess(80.0f);
    }
    return found->second;
}

Magnum::SceneGraph::Drawable3D* DrawableCache::CreateDrawableFromMesh(MaterialCache& mMaterialCache,
                                                                      MeshInfo& meshInfo,
                                                                      ObjectInfo& objectInfo,
                                                                      int32_t materialId,
                                                                      uint32_t lightCount,
                                                                      bool shadeless) {
    /* Add drawables for objects that have a mesh, again ignoring objects
       that are not part of the hierarchy. There can be multiple mesh
       assignments for one object, simply add one drawable for each. */
    const auto object = objectInfo.object;
    const auto objectId = objectInfo.id;
    auto& mesh = meshInfo.mesh;

    if (!object || !mesh) {
        return nullptr;
    }

    objectInfo.meshIds.push_back(meshInfo.id);

    Shaders::PhongGL::Flags flags;
    if (meshInfo.hasVertexColors) {
        flags |= Shaders::PhongGL::Flag::VertexColor;
    }
    if (meshInfo.hasSeparateBitangents) {
        flags |= Shaders::PhongGL::Flag::Bitangent;
    }

    /* Material not available / not loaded. If the mesh has vertex
       colors, use that, otherwise apply a default material; use a flat
       shader for lines / points */
    if (materialId == -1 || !mMaterialCache.mMaterials[materialId].raw.has_value()) {
        if (mesh->primitive() == GL::MeshPrimitive::Triangles ||
            mesh->primitive() == GL::MeshPrimitive::TriangleStrip ||
            mesh->primitive() == GL::MeshPrimitive::TriangleFan) {
            return new PhongDrawable{
                *object, PhongShader(flags, lightCount), *mesh, objectId, 0xffffff_rgbf, shadeless, mOpaqueDrawables};
        } else {
            return new FlatDrawable{*object,
                                    FlatShader((meshInfo.hasVertexColors ? Shaders::FlatGL3D::Flag::VertexColor
                                                                         : Shaders::FlatGL3D::Flags{})),
                                    *mesh,
                                    objectId,
                                    0xffffff_rgbf,
                                    Vector3{Constants::nan()},
                                    mOpaqueDrawables};
        }

        /* Material available */
    } else {
        const Trade::PhongMaterialData& material = *mMaterialCache.mMaterials[materialId].Phong();

        if (material.isDoubleSided()) {
            flags |= Shaders::PhongGL::Flag::DoubleSided;
        }

        /* Textured material. If the texture failed to load, again just
           use a default-colored material. */
        GL::Texture2D* diffuseTexture = nullptr;
        GL::Texture2D* normalTexture = nullptr;
        float normalTextureScale = 1.0f;
        if (material.hasAttribute(Trade::MaterialAttribute::DiffuseTexture)) {
            std::optional<GL::Texture2D>& texture = mMaterialCache.mTextures[material.diffuseTexture()].texture;
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
            std::optional<GL::Texture2D>& texture = mMaterialCache.mTextures[material.normalTexture()].texture;
            /* If there are no tangents, the mesh would render all
               black. Ignore the normal map in that case. */
            /** @todo generate tangents instead once we have the algo */
            if (!meshInfo.hasTangents) {
                kitgui::log::error(
                    fmt::format("Mesh {} doesn't have tangents, auto-generation NYI, so ignoring normal map for now",
                                meshInfo.debugName));
            } else if (texture) {
                normalTexture = &*texture;
                normalTextureScale = material.normalTextureScale();
                flags |= Shaders::PhongGL::Flag::NormalTexture;
                if (material.hasTextureTransformation())
                    flags |= Shaders::PhongGL::Flag::TextureTransformation;
            }
        }

        Magnum::SceneGraph::DrawableGroup3D* group = &mOpaqueDrawables;
        if (material.alphaMode() == Trade::MaterialAlphaMode::Blend) {
            group = &mTransparentDrawables;
        }

        return new PhongDrawable{*object,
                                 PhongShader(flags, lightCount),
                                 *mesh,
                                 objectId,
                                 material.diffuseColor(),
                                 diffuseTexture,
                                 normalTexture,
                                 normalTextureScale,
                                 material.alphaMask(),
                                 material.commonTextureMatrix(),
                                 shadeless,
                                 *group};
    }
}

}  // namespace kitgui
