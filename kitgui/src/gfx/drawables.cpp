#include "gfx/drawables.h"

#include <Magnum/GL/Renderer.h>
#include <Magnum/SceneGraph/Camera.h>
#include <Magnum/Shaders/PhongGL.h>
#include <Magnum/Trade/MaterialData.h>
#include <Magnum/Trade/PbrMetallicRoughnessMaterialData.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <imgui.h>
#include <vector>

#include "gfx/materials.h"
#include "gfx/sceneGraph.h"

using namespace Magnum;
using namespace Magnum::Math::Literals;
using namespace Magnum::Math::Literals::ColorLiterals;

namespace kitgui {

PhongDrawable::PhongDrawable(const SharedDrawableState& state,
                             Object3D& object,
                             uint32_t objectId,
                             GL::Mesh& mesh,
                             Shaders::PhongGL& shader,
                             const Color4& color,
                             GL::Texture2D* diffuseTexture,
                             GL::Texture2D* normalTexture,
                             Float normalTextureScale,
                             Float alphaMask,
                             const Matrix3& textureMatrix,
                             float ambientFactor,
                             float specularFactor,
                             float shininess,
                             bool shadeless,
                             SceneGraph::DrawableGroup3D& group)
    : BaseDrawable{object, &group},
      mTextureMatrix(textureMatrix),
      mShared(state),
      mShader(shader),
      mMesh(mesh),
      mObjectId(objectId),
      mColor(color),
      mDiffuseTexture(diffuseTexture),
      mNormalTexture(normalTexture),
      mNormalTextureScale(normalTextureScale),
      mAlphaMask(alphaMask),
      mAmbientFactor(ambientFactor),
      mSpecularFactor(specularFactor),
      mShininess(shininess),
      mShadeless(shadeless) {}

void PhongDrawable::draw(const Matrix4& transformationMatrix, SceneGraph::Camera3D& camera) {
    if (!mEnabled) {
        return;
    }
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

    if (mShadeless || mShared.shadeless) {
        // 100% ambient, 0% diffuse means lights will have no contribution to the final color
        mShader.setAmbientColor(mColor * mAmbientFactor)
            .setDiffuseColor(0x00000000_rgbaf)
            .setSpecularColor(0x00000000_rgbaf);
    } else {
        mShader.setAmbientColor(mColor * mAmbientFactor * mShared.ambientFactor)
            .setDiffuseColor(mColor)
            .setSpecularColor(0xffffff00_rgbaf * mSpecularFactor * mShared.specularFactor)
            .setShininess(mShared.shininessDefault * mShininess);
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
    if (ImGui::TreeNode(this, "%d", mObjectId)) {
        ImGui::Text("PhongGL");
        ImGui::Checkbox("enabled", &mEnabled);
        ImVec4 c = {mColor.r(), mColor.g(), mColor.b(), mColor.a()};
        ImGui::ColorButton("color", c);
        ImGui::LabelText("hasDiffuse", "%s", mDiffuseTexture == nullptr ? "false" : "true");
        ImGui::LabelText("hasNormal", "%s", mNormalTexture == nullptr ? "false" : "true");
        ImGui::InputFloat("ambientFactor", &mAmbientFactor);
        ImGui::InputFloat("specularFactor", &mSpecularFactor);
        ImGui::InputFloat("shininess", &mShininess);
        ImGui::Checkbox("shadeless", &mShadeless);
        ImGui::LabelText("normalTextureScale", "%f", mNormalTextureScale);
        ImGui::LabelText("alphamask", "%f", mAlphaMask);
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
void LightDrawable::ImGui() {}

Shaders::PhongGL& Drawables::GetOrCreatePhongShader(Shaders::PhongGL::Flags flags, uint32_t lightCount) {
    auto found = mPhongShaders.find(enumCastUnderlyingType(flags));
    if (found == mPhongShaders.end()) {
        Shaders::PhongGL::Configuration configuration;
        configuration.setFlags(Shaders::PhongGL::Flag::ObjectId | flags).setLightCount(lightCount);
        found = mPhongShaders.emplace(enumCastUnderlyingType(flags), Shaders::PhongGL{configuration}).first;
    }
    return found->second;
}

Magnum::SceneGraph::Drawable3D* Drawables::CreateDrawableFromMesh(MaterialCache& mMaterialCache,
                                                                  MeshInfo& meshInfo,
                                                                  ObjectInfo& objectInfo,
                                                                  const MaterialInfo* materialInfo,
                                                                  uint32_t lightCount) {
    /* Add drawables for objects that have a mesh, again ignoring objects
       that are not part of the hierarchy. There can be multiple mesh
       assignments for one object, simply add one drawable for each. */
    Object3D* object = objectInfo.object;
    uint32_t objectId = objectInfo.id;
    auto& mesh = meshInfo.mesh;

    if (!object || !mesh) {
        return nullptr;
    }

    objectInfo.meshIds.push_back(meshInfo.id);

    Shaders::PhongGL::Flags flags{};
    if (meshInfo.hasVertexColors) {
        flags |= Shaders::PhongGL::Flag::VertexColor;
    }
    if (meshInfo.hasSeparateBitangents) {
        flags |= Shaders::PhongGL::Flag::Bitangent;
    }

    if (!materialInfo || !materialInfo->raw) {
        /* Material not available */
        return new PhongDrawable(mState, *object, objectId, *mesh, GetOrCreatePhongShader(flags, lightCount),
                                 0xffffff_rgbf, nullptr, nullptr, 0.0f, 0.5f, {}, 1.0f, 0.0f, 80.0f, false,
                                 mOpaqueDrawables);
    } else {
        /* Material available */
        const Trade::PhongMaterialData& material = *materialInfo->Phong();
        const Trade::PbrMetallicRoughnessMaterialData* pbrMat = materialInfo->Pbr();

        if (material.isDoubleSided()) {
            flags |= Shaders::PhongGL::Flag::DoubleSided;
        }
        if (material.hasTextureTransformation()) {
            flags |= Shaders::PhongGL::Flag::TextureTransformation;
        }

        GL::Texture2D* emissiveTexture = nullptr;
        Shaders::PhongGL::Flags emissiveFlags{flags};
        if (pbrMat && pbrMat->hasAttribute(Trade::MaterialAttribute::EmissiveTexture)) {
            std::optional<GL::Texture2D>& texture = mMaterialCache.mTextures[pbrMat->emissiveTexture()].texture;
            if (texture) {
                emissiveTexture = &texture.value();
                emissiveFlags |= Shaders::PhongGL::Flag::AmbientTexture | Shaders::PhongGL::Flag::DiffuseTexture;
            }
        }

        if (material.alphaMode() == Trade::MaterialAlphaMode::Mask) {
            flags |= Shaders::PhongGL::Flag::AlphaMask;
        }

        GL::Texture2D* diffuseTexture = nullptr;
        if (material.hasAttribute(Trade::MaterialAttribute::DiffuseTexture)) {
            std::optional<GL::Texture2D>& texture = mMaterialCache.mTextures[material.diffuseTexture()].texture;
            if (texture) {
                diffuseTexture = &texture.value();
                flags |= Shaders::PhongGL::Flag::AmbientTexture | Shaders::PhongGL::Flag::DiffuseTexture;
            }
        }

        GL::Texture2D* normalTexture = nullptr;
        float normalTextureScale = 1.0f;
        if (material.hasAttribute(Trade::MaterialAttribute::NormalTexture)) {
            std::optional<GL::Texture2D>& texture = mMaterialCache.mTextures[material.normalTexture()].texture;
            /* If there are no tangents, the mesh would render all
               black. Ignore the normal map in that case. */
            if (!meshInfo.hasTangents) {
                Error{} << "Mesh " << meshInfo.debugName.c_str() << "doesn't have tangents, so ignoring normal map";
            } else if (texture) {
                normalTexture = &texture.value();
                normalTextureScale = material.normalTextureScale();
                flags |= Shaders::PhongGL::Flag::NormalTexture;
            }
        }

        Magnum::SceneGraph::DrawableGroup3D* group = &mOpaqueDrawables;
        if (material.alphaMode() == Trade::MaterialAlphaMode::Blend) {
            group = &mTransparentDrawables;
        }

        // TODO: derive from pbr?
        constexpr float kAmbient = 1.0f;
        constexpr float kSpecular = 1.0f;
        constexpr float kShininess = 1.0f;

        if (pbrMat && pbrMat->emissiveColor() != 0x000000_srgbf) {
            // also create emissive drawable
            // 100% ambient, 0% diffuse, shadeless.
            new PhongDrawable(mState, *object, objectId, *mesh, GetOrCreatePhongShader(emissiveFlags, lightCount),
                              pbrMat->emissiveColor(), emissiveTexture, nullptr, {}, material.alphaMask(),
                              material.commonTextureMatrix(), 1.0f, 0.0f, 0.0f, true, mEmissiveDrawables);
        }

        return new PhongDrawable(mState, *object, objectId, *mesh, GetOrCreatePhongShader(flags, lightCount),
                                 material.diffuseColor(), diffuseTexture, normalTexture, normalTextureScale,
                                 material.alphaMask(), material.commonTextureMatrix(), kAmbient, kSpecular, kShininess,
                                 false, *group);
    }
}

}  // namespace kitgui
