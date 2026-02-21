#pragma once

#include <Corrade/Containers/ArrayView.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Matrix3.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/SceneGraph/Drawable.h>
#include <Magnum/SceneGraph/TranslationRotationScalingTransformation3D.h>
#include <Magnum/Shaders/MeshVisualizerGL.h>
#include <Magnum/Shaders/PhongGL.h>

#include <span>
#include <vector>

#include "gfx/meshes.h"
#include "gfx/sceneGraph.h"

namespace kitgui {
struct MaterialCache;
struct MaterialInfo;
}  // namespace kitgui

namespace kitgui {

struct SharedDrawableState {
    bool shadeless = false;
    float ambientFactor = 0.05f;
    float specularFactor = 0.4f;
    float shininessDefault = 80.0f;
};

class BaseDrawable : public Magnum::SceneGraph::Drawable3D {
   public:
    explicit BaseDrawable(Object3D& object, Magnum::SceneGraph::DrawableGroup3D* group)
        : Magnum::SceneGraph::Drawable3D(object, group) {}
    virtual void ImGui() = 0;
};

class PhongDrawable : public BaseDrawable {
   public:
    explicit PhongDrawable(const SharedDrawableState& state,
                           Object3D& object,
                           uint32_t objectId,
                           Magnum::GL::Mesh& mesh,
                           Magnum::Shaders::PhongGL& shader,
                           const Magnum::Color4& color,
                           Magnum::GL::Texture2D* diffuseTexture,
                           Magnum::GL::Texture2D* normalTexture,
                           float normalTextureScale,
                           float alphaMask,
                           const Magnum::Matrix3& textureMatrix,
                           float ambientFactor,
                           float specularFactor,
                           float shininess,
                           bool shadeless,
                           Magnum::SceneGraph::DrawableGroup3D& group);

    uint32_t getObjectId() const { return mObjectId; }
    void setAmbientFactor(float factor) { mAmbientFactor = factor; }

   private:
    void draw(const Magnum::Matrix4& transformationMatrix, Magnum::SceneGraph::Camera3D& camera) override;
    void ImGui() override;

    Magnum::Matrix3 mTextureMatrix;
    const SharedDrawableState& mShared;
    Magnum::Shaders::PhongGL& mShader;
    Magnum::GL::Mesh& mMesh;
    uint32_t mObjectId;
    Magnum::Color4 mColor;
    Magnum::GL::Texture2D* mDiffuseTexture;
    Magnum::GL::Texture2D* mNormalTexture;
    float mNormalTextureScale;
    float mAlphaMask;
    float mAmbientFactor;
    float mSpecularFactor;
    float mShininess;
    bool mShadeless;
    bool mEnabled = true;
};

class LightDrawable : public BaseDrawable {
   public:
    explicit LightDrawable(Object3D& object,
                           bool directional,
                           std::vector<Magnum::Vector4>& positions,
                           Magnum::SceneGraph::DrawableGroup3D& group);

   private:
    void draw(const Magnum::Matrix4& transformationMatrix, Magnum::SceneGraph::Camera3D&) override;
    void ImGui() override;

    bool mDirectional;
    std::vector<Magnum::Vector4>& mPositions;
};

struct Drawables {
    Magnum::Shaders::PhongGL& GetOrCreatePhongShader(Magnum::Shaders::PhongGL::Flags flags, uint32_t lightCount);
    Magnum::SceneGraph::Drawable3D* CreateDrawableFromMesh(MaterialCache& mMaterialCache,
                                                           MeshInfo& meshInfo,
                                                           ObjectInfo& objectInfo,
                                                           const MaterialInfo* materialInfo,
                                                           uint32_t lightCount);

    void SetLightColors(const std::span<const Magnum::Color3>& colors) {
        const Corrade::Containers::ArrayView<const Magnum::Color3> colorsView(colors.data(), colors.size());
        for (auto& pair : mPhongShaders) {
            pair.second.setLightColors(colorsView);
            pair.second.setLightSpecularColors(colorsView);
        }
    }
    void SetLightPositions(const std::span<const Magnum::Vector4>& positions) {
        const Corrade::Containers::ArrayView<const Magnum::Vector4> positionsView(positions.data(), positions.size());
        for (auto& pair : mPhongShaders) {
            pair.second.setLightPositions(positionsView);
        }
    }
    void SetLightRanges(const std::span<const float>& ranges) {
        const Corrade::Containers::ArrayView<const float> rangesView(ranges.data(), ranges.size());
        for (auto& pair : mPhongShaders) {
            pair.second.setLightRanges(rangesView);
        }
    }
    void SetShadeless(bool shadeless) { mState.shadeless = shadeless; }
    void SetAmbientFactor(float ambientFactor) { mState.ambientFactor = ambientFactor; }
    // uint32_t = Shaders::PhongGL::Flags
    std::unordered_map<uint32_t, Magnum::Shaders::PhongGL> mPhongShaders;
    // uint32_t = Shaders::MeshVisualizer3D::Flags
    std::unordered_map<uint32_t, Magnum::Shaders::MeshVisualizerGL3D> mMeshVisualizerShaders;

    SharedDrawableState mState{};

    Magnum::SceneGraph::DrawableGroup3D mOpaqueDrawables;
    Magnum::SceneGraph::DrawableGroup3D mTransparentDrawables;
    Magnum::SceneGraph::DrawableGroup3D mEmissiveDrawables;
    Magnum::SceneGraph::DrawableGroup3D mLightDrawables;
};

}  // namespace kitgui
